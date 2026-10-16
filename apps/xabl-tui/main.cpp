// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
// Keyboard-first terminal IDE. ncurses is an optional frontend only;
// parsing, bytecode and execution remain in the shared xabl_core library.
#include "editor_buffer.hpp"
#include "file_browser.hpp"
#include "syntax_highlight.hpp"

#include <xabl/runtime/xabl.hpp>
#include <xabl/runtime/debugger.hpp>

#include <curses.h>
#include <algorithm>
#include <charconv>
#include <cctype>
#include <array>
#include <clocale>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace {
// Usual xterm/Konsole modified-function-key sequences. If a terminal
// cannot transmit them, the F10 command menu offers the same operations.
constexpr int key_reset = KEY_MAX + 1;
constexpr int key_breakpoint = KEY_MAX + 2;
constexpr int key_run = KEY_MAX + 3;
constexpr int key_variables = KEY_MAX + 4;
constexpr int key_output = KEY_MAX + 5;
constexpr int key_compile = KEY_MAX + 6;
constexpr int key_exit = KEY_MAX + 7;

class CursesSession {
public:
    CursesSession() {
        if (!initscr()) throw std::runtime_error("cannot initialise terminal");
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(1);
#ifdef NCURSES_VERSION
        define_key("\033[1;5Q", key_reset);    // Ctrl+F2
        define_key("\033[12;5~", key_reset);   // alternate F2 encoding
        define_key("\033[19;5~", key_breakpoint); // Ctrl+F8
        define_key("\033[20;5~", key_run);     // Ctrl+F9
        define_key("\033[18;5~", key_variables); // Ctrl+F7
        define_key("\033[15;3~", key_output);  // Alt+F5
        define_key("\033[20;3~", key_compile); // Alt+F9
        define_key("\033x", key_exit);         // Alt+X
        define_key("\033X", key_exit);
#endif
#if defined(__unix__) || defined(__APPLE__)
        // Let the IDE receive Ctrl+S/Q (otherwise XOFF/XON) and Ctrl+Z
        // (otherwise terminal suspend). Curses restores the original
        // terminal settings when the editor exits normally.
        termios mode{};
        if (tcgetattr(STDIN_FILENO, &mode) == 0) {
            mode.c_iflag &= static_cast<tcflag_t>(~(IXON | IXOFF));
#if defined(VSUSP) && defined(_POSIX_VDISABLE)
            mode.c_cc[VSUSP] = _POSIX_VDISABLE;
#endif
            tcsetattr(STDIN_FILENO, TCSANOW, &mode);
        }
#endif
        if (has_colors()) {
            start_color();
            use_default_colors();
            init_pair(1, COLOR_WHITE, COLOR_BLUE);
            init_pair(2, COLOR_BLACK, COLOR_CYAN);
            init_pair(3, COLOR_YELLOW, COLOR_BLUE);
            init_pair(4, COLOR_WHITE, COLOR_BLACK);
            init_pair(5, COLOR_YELLOW, COLOR_BLUE);
            init_pair(6, COLOR_GREEN, COLOR_BLUE);
            init_pair(7, COLOR_CYAN, COLOR_BLUE);
            init_pair(8, COLOR_MAGENTA, COLOR_BLUE);
            init_pair(9, COLOR_WHITE, COLOR_BLUE);
        }
    }
    ~CursesSession() { endwin(); }
    CursesSession(const CursesSession&) = delete;
    CursesSession& operator=(const CursesSession&) = delete;
};

constexpr int ctrl(char c) { return c - '@'; }
constexpr int editor_top = 2;
constexpr int gutter_width = 6;

class TextStudio {
public:
    explicit TextStudio(const fs::path& source) {
        if (!source.empty()) document_.load(source);
    }

    int run() {
        CursesSession session;
        message_ = "Welcome to XABL Text Studio. F1 for help.";
        while (active_) {
            render();
            try { handle(getch()); }
            catch (const std::exception& e) { message_ = std::string("Error: ") + e.what(); }
        }
        return 0;
    }

private:
    enum class Mode { Editor, Output, Help, Browser, Variables, Menu };
    xabl::tui::EditorBuffer document_;
    std::unique_ptr<xabl::tui::FileBrowser> browser_;
    std::size_t browser_scroll_{};
    Mode mode_{Mode::Editor};
    std::string message_;
    std::string output_;
    std::string last_search_;
    std::optional<std::size_t> last_compile_error_line_;
    std::size_t scroll_row_{};
    std::size_t scroll_column_{};
    std::size_t output_scroll_{};
    std::set<std::size_t> breakpoint_lines_;
    std::ostringstream debug_output_;
    // Program and map must outlive the VM session borrowing their contents.
    std::unique_ptr<xabl::Program> debug_program_;
    std::unique_ptr<xabl::BreakpointMap> debug_breakpoints_;
    std::unique_ptr<xabl::Vm> debug_vm_;
    bool active_{true};
    bool zoomed_{false};

    [[nodiscard]] bool debugging() const noexcept {
        return debug_vm_ && debug_vm_->debugging();
    }

    void reset_debugger() {
        if (debug_vm_) debug_vm_->cancel_debugging();
        debug_vm_.reset();
        debug_breakpoints_.reset();
        debug_program_.reset();
    }

    static void paint(int y, int x, std::string_view text, int width) {
        if (y < 0 || x < 0 || width <= 0) return;
        const std::string clipped(text.substr(0, static_cast<std::size_t>(width)));
        mvaddnstr(y, x, clipped.c_str(), width);
    }

    static std::string safe_title(std::string title) {
        for (char& character : title) {
            const auto byte = static_cast<unsigned char>(character);
            if (byte < 32 || byte == 127) character = '?';
        }
        return title;
    }

    std::string variables_text() const {
        if (!debugging()) return "No active debugger session.\n";
        std::vector<std::pair<std::string, std::string>> rows;
        for (const auto& [name, value] : debug_vm_->variables())
            rows.emplace_back(name, safe_title(value.as_string()));
        std::sort(rows.begin(), rows.end());
        std::ostringstream view;
        view << "Variables (" << rows.size() << ")\n\n";
        for (const auto& [name, value] : rows)
            view << name << " = " << value << '\n';
        return view.str();
    }

    void paint_source(int y, const std::string& source, int width) {
        if (scroll_column_ >= source.size() || width <= 0) return;
        const std::size_t visible = std::min(static_cast<std::size_t>(width),
                                             source.size() - scroll_column_);
        attrset(COLOR_PAIR(1));
        paint(y, gutter_width,
              std::string_view(source).substr(scroll_column_, visible), width);
        for (const auto& span : xabl::tui::highlight_line(source)) {
            const auto begin = std::max(span.start, scroll_column_);
            const auto end = std::min(span.start + span.length,
                                      scroll_column_ + visible);
            if (end <= begin) continue;
            int colour = 1;
            switch (span.kind) {
            case xabl::tui::SyntaxKind::Keyword: colour = 5; break;
            case xabl::tui::SyntaxKind::Function:
            case xabl::tui::SyntaxKind::Logical: colour = 9; break;
            case xabl::tui::SyntaxKind::String: colour = 6; break;
            case xabl::tui::SyntaxKind::Comment: colour = 7; break;
            case xabl::tui::SyntaxKind::Number: colour = 8; break;
            case xabl::tui::SyntaxKind::Plain: break;
            }
            attrset(COLOR_PAIR(colour) |
                    (span.kind == xabl::tui::SyntaxKind::Keyword ? A_BOLD : 0));
            paint(y, gutter_width + static_cast<int>(begin - scroll_column_),
                  std::string_view(source).substr(begin, end - begin),
                  static_cast<int>(end - begin));
        }
        attrset(COLOR_PAIR(1));
    }

    void bar(int y, std::string_view value, int pair) const {
        int h{}, w{};
        getmaxyx(stdscr, h, w);
        if (y < 0 || y >= h) return;
        attrset(COLOR_PAIR(pair) | A_BOLD);
        mvhline(y, 0, ' ', w);
        paint(y, 1, value, w - 2);
        attrset(COLOR_PAIR(1));
    }

    void ensure_visible(int rows, int width) {
        if (document_.cursor().row < scroll_row_)
            scroll_row_ = document_.cursor().row;
        if (document_.cursor().row >= scroll_row_ + static_cast<std::size_t>(rows))
            scroll_row_ = document_.cursor().row - static_cast<std::size_t>(rows) + 1;
        if (document_.cursor().column < scroll_column_)
            scroll_column_ = document_.cursor().column;
        if (document_.cursor().column >= scroll_column_ + static_cast<std::size_t>(width))
            scroll_column_ = document_.cursor().column - static_cast<std::size_t>(width) + 1;
    }

    void render() {
        int height{}, width{};
        getmaxyx(stdscr, height, width);
        bkgd(COLOR_PAIR(1));
        erase();
        if (width < 45 || height < 12) {
            bar(0, "XABL TEXT STUDIO", 2);
            paint(2, 2, "Enlarge terminal (minimum 45x12).", width - 4);
            paint(height - 1, 1, "Alt+X Exit | F10 Menu", width - 2);
            refresh();
            return;
        }

        std::string heading = " XABL Text Studio  |  ";
        heading += mode_ == Mode::Editor ? "EDITOR" :
                   mode_ == Mode::Output ? "PROGRAM OUTPUT" :
                   mode_ == Mode::Browser ? "FILE BROWSER" :
                   mode_ == Mode::Variables ? "DEBUG VARIABLES" :
                   mode_ == Mode::Menu ? "MENU" : "HELP";
        const bool expanded = zoomed_ && mode_ == Mode::Editor;
        if (!expanded) bar(0, heading, 2);

        const std::string filename = document_.path().empty()
            ? "UNTITLED.PRG" : document_.path().string();
        if (!expanded)
            bar(1, mode_ == Mode::Browser && browser_
                ? " Directory: " + safe_title(browser_->directory().string())
                : " File: " + safe_title(filename) +
                  (document_.dirty() ? "  [Modified]" : ""), 1);

        const int top = expanded ? 0 : editor_top;
        const int viewport_rows = height - (expanded ? 3 : 5);
        const int viewport_width = width - gutter_width - 1;
        attrset(COLOR_PAIR(mode_ == Mode::Output || mode_ == Mode::Variables
                               ? 4 : 1));

        if (mode_ == Mode::Editor) {
            ensure_visible(viewport_rows, viewport_width);
            const auto paused_at = debugging() ? debug_vm_->debug_location()
                                                : std::nullopt;
            for (int visible = 0; visible < viewport_rows; ++visible) {
                const std::size_t index = scroll_row_ + visible;
                if (index >= document_.lines().size()) break;
                std::ostringstream label;
                label.width(4);
                const bool current = paused_at && paused_at->line == index + 1;
                label << (index + 1)
                      << (breakpoint_lines_.contains(index + 1) ? '*' : ' ')
                      << (current ? '>' : '|');
                if (current) attron(A_BOLD);
                else attron(A_DIM);
                paint(top + visible, 0, label.str(), gutter_width);
                if (current) attroff(A_BOLD);
                else attroff(A_DIM);
                const auto& source = document_.lines()[index];
                paint_source(top + visible, source, viewport_width);
            }
        } else if (mode_ == Mode::Browser && browser_) {
            const auto& entries = browser_->entries();
            if (browser_->selected() < browser_scroll_)
                browser_scroll_ = browser_->selected();
            if (browser_->selected() >= browser_scroll_ +
                    static_cast<std::size_t>(viewport_rows))
                browser_scroll_ = browser_->selected() -
                    static_cast<std::size_t>(viewport_rows) + 1;
            for (int visible = 0; visible < viewport_rows; ++visible) {
                const std::size_t selected = browser_scroll_ + static_cast<std::size_t>(visible);
                if (selected >= entries.size()) break;
                const bool active = selected == browser_->selected();
                attrset(COLOR_PAIR(active ? 2 : 1) | (active ? A_BOLD : 0));
                mvhline(top + visible, 0, ' ', width);
                paint(top + visible, 2, entries[selected].display_name, width - 4);
            }
            attrset(COLOR_PAIR(1));
        } else {
            std::vector<std::string> rows;
            std::istringstream stream(mode_ == Mode::Output ? output_ :
                                       mode_ == Mode::Variables ? variables_text() :
                                       mode_ == Mode::Menu ? menu_text() : help_text());
            std::string line;
            while (std::getline(stream, line)) rows.push_back(line);
            output_scroll_ = std::min(output_scroll_, rows.size());
            for (int visible = 0; visible < viewport_rows; ++visible) {
                const auto row = output_scroll_ + static_cast<std::size_t>(visible);
                if (row < rows.size())
                    paint(top + visible, 2, rows[row], width - 4);
            }
        }

        std::ostringstream status;
        status << " " << (mode_ == Mode::Editor ? "Edit" :
                           mode_ == Mode::Output ? "Output" :
                           mode_ == Mode::Browser ? "Browse" :
                           mode_ == Mode::Variables ? "Variables" :
                           mode_ == Mode::Menu ? "Menu" : "Help")
               << "  Line " << document_.cursor().row + 1
               << "  Col " << document_.cursor().column + 1
               << "  " << document_.lines().size() << " lines"
               << (document_.dirty() ? "  * UNSAVED *" : "")
               << (debugging() ? "  [DEBUG PAUSED]" : "")
               << (breakpoint_lines_.empty()
                       ? "" : "  BPs " + std::to_string(breakpoint_lines_.size()));
        bar(height - 3, status.str(), 2);
        bar(height - 2, " " + message_, 1);
        bar(height - 1,
            mode_ == Mode::Browser
                ? " Arrows Move Enter Open Backspace Parent Esc Editor"
                : " F1 Help F2 Save F3 Open F4 Cursor F5 Zoom F6 Out F7 Trace F8 Step F9 Build F10 Menu",
            2);
        if (mode_ == Mode::Editor) {
            curs_set(1);
            const auto cursor = document_.cursor();
            move(static_cast<int>(cursor.row - scroll_row_) + top,
                 static_cast<int>(cursor.column - scroll_column_) + gutter_width);
        } else {
            curs_set(0);
            move(height - 2, 1);
        }
        refresh();
    }

    static std::string help_text() {
        return
            "XABL TEXT STUDIO | Borland-inspired shortcuts\n"
            "\n"
            " F1 Help                 F2 Save            F3 Browse/Open\n"
            " Ctrl+N New              F4 Run to cursor   F5 Zoom editor\n"
            " F6 Output/editor        F7 Trace          F8 Step\n"
            " F9 Build/check          Alt+F9 Compile    F10 Menu\n"
            " Ctrl+F9 Run/continue    Ctrl+F2 Reset     Ctrl+F8 Breakpoint\n"
            " Ctrl+F7 Variables       Alt+F5 Output     Alt+X Exit\n"
            " Ctrl+O Open path        Ctrl+F Find       Ctrl+G Go to line\n"
            " Ctrl+E/F12 Error jump   Ctrl+S Save       Ctrl+Z Undo\n"
            " Ctrl+Q Exit             Esc Back to editor\n"
            "\n"
            " F7 and F8 both step ONE SOURCE STATEMENT for now.\n"
            " Procedure step-into/step-over distinction is not implemented.\n"
            " F9 and Alt+F9 both check the one active source file.\n"
            " If modified function keys do not work, use F10's command menu.\n"
            "\n"
            " Run/step may write DBF data; reset does NOT undo prior writes.\n"
            " Gutter: * breakpoint, > next paused statement.\n";
    }

    static std::string menu_text() {
        return
            "XABL COMMAND MENU | Press a letter (Esc closes)\n"
            "\n"
            " FILE       S Save       O Browse/Open    P Open path   N New\n"
            " BUILD      C Check      R Run/Continue\n"
            " DEBUG      B Breakpoint T Step           G Run to cursor\n"
            "            V Variables D Stop/Reset\n"
            " VIEW       U Output     Z Zoom           H Help\n"
            " NAVIGATE   F Find       L Go to line     E Compiler error\n"
            " EXIT       X Quit\n"
            "\n"
            " Works when the terminal cannot send modified function keys.\n";
    }

    std::string prompt(std::string_view question) {
        int height{}, width{};
        getmaxyx(stdscr, height, width);
        if (height < 12 || width < 45) return {};
        bar(height - 2, std::string(question), 2);
        move(height - 2, static_cast<int>(question.size()) + 2);
        echo();
        curs_set(1);
        char answer[1024]{};
        const int result = getnstr(answer, sizeof(answer) - 1);
        noecho();
        return result == ERR ? std::string{} : std::string(answer);
    }

    bool confirm(std::string_view question) {
        message_ = std::string(question) + "  [y/N]";
        render();
        const int key = getch();
        return key == 'y' || key == 'Y';
    }

    bool discard_ok() {
        return !document_.dirty() ||
               confirm("Discard changes to the current document?");
    }

    void save() {
        fs::path destination = document_.path();
        if (destination.empty()) {
            const auto name = prompt("Save as (path):");
            if (name.empty()) { message_ = "Save cancelled."; return; }
            destination = name;
            if (fs::exists(destination) && !confirm("Overwrite existing file?")) {
                message_ = "Save cancelled.";
                return;
            }
        }
        document_.save(destination);
        message_ = "Saved " + destination.string();
    }

    void open() {
        const auto path = prompt("Open source file (path):");
        if (path.empty()) return;
        if (!discard_ok()) return;
        document_.load(path);
        scroll_row_ = scroll_column_ = 0;
        mode_ = Mode::Editor;
        last_compile_error_line_.reset();
        reset_debugger();
        breakpoint_lines_.clear();
        message_ = "Opened " + path;
    }

    void browse() {
        const auto start = document_.path().empty()
            ? fs::current_path() : fs::absolute(document_.path()).parent_path();
        browser_ = std::make_unique<xabl::tui::FileBrowser>(start);
        browser_scroll_ = 0;
        mode_ = Mode::Browser;
        message_ = "Select .PRG or .XABL. Enter opens; Esc cancels.";
    }

    void browse_key(int key) {
        if (!browser_) { mode_ = Mode::Editor; return; }
        switch (key) {
        case KEY_UP: browser_->move_up(); break;
        case KEY_DOWN: browser_->move_down(); break;
        case KEY_HOME: browser_->select(0); break;
        case KEY_END:
            if (!browser_->entries().empty())
                browser_->select(browser_->entries().size() - 1);
            break;
        case KEY_PPAGE:
            for (int i = 0; i < 12; ++i) browser_->move_up();
            break;
        case KEY_NPAGE:
            for (int i = 0; i < 12; ++i) browser_->move_down();
            break;
        case KEY_BACKSPACE:
        case 127:
        case 8:
            browser_->parent();
            browser_scroll_ = 0;
            break;
        case KEY_ENTER:
        case '\n':
        case '\r': {
            const auto selected = browser_->activate();
            if (selected.empty()) { browser_scroll_ = 0; break; }
            if (!discard_ok()) break;
            document_.load(selected);
            mode_ = Mode::Editor;
            browser_.reset();
            scroll_row_ = scroll_column_ = 0;
            last_compile_error_line_.reset();
            reset_debugger();
            breakpoint_lines_.clear();
            message_ = "Opened " + safe_title(selected.string());
            break;
        }
        case 27:
            mode_ = Mode::Editor;
            browser_.reset();
            break;
        case ctrl('O'):
            mode_ = Mode::Editor;
            browser_.reset();
            open();
            break;
        default: break;
        }
    }

    void new_document() {
        if (!discard_ok()) return;
        document_.clear();
        scroll_row_ = scroll_column_ = 0;
        mode_ = Mode::Editor;
        last_compile_error_line_.reset();
        reset_debugger();
        breakpoint_lines_.clear();
        message_ = "New source file.";
    }

    void goto_prompt() {
        const auto supplied = prompt("Go to line (number):");
        if (supplied.empty()) return;
        std::size_t value{};
        const auto [end, code] = std::from_chars(supplied.data(),
                                                 supplied.data() + supplied.size(), value);
        if (code != std::errc{} || end != supplied.data() + supplied.size() ||
            !document_.go_to_line(value)) {
            message_ = "Invalid source line number.";
            return;
        }
        mode_ = Mode::Editor;
        message_ = "Line " + std::to_string(value);
    }

    void next_or_error() {
        if (mode_ == Mode::Output && last_compile_error_line_) {
            const auto target = *last_compile_error_line_;
            if (document_.go_to_line(target)) {
                mode_ = Mode::Editor;
                message_ = "Compiler error at line " + std::to_string(target);
            } else {
                message_ = "Reported line is outside the current source.";
            }
            return;
        }
        search(true);
    }

    void toggle_breakpoint() {
        const std::size_t line = document_.cursor().row + 1;
        if (breakpoint_lines_.contains(line)) {
            breakpoint_lines_.erase(line);
            if (debugging()) debug_breakpoints_->disable(line);
            message_ = "Breakpoint removed from line " + std::to_string(line);
            return;
        }

        // Compiling for a breakpoint is deliberately non-executing.
        if (debugging()) {
            if (!debug_breakpoints_->instruction_at(line)) {
                message_ = "No executable statement on line " + std::to_string(line);
                return;
            }
            debug_breakpoints_->enable(line);
        } else {
            const auto compiled = xabl::Compiler{}.compile(document_.text());
            if (!xabl::BreakpointMap(compiled).instruction_at(line)) {
                message_ = "No executable statement on line " + std::to_string(line);
                return;
            }
        }
        breakpoint_lines_.insert(line);
        message_ = "Breakpoint set on line " + std::to_string(line);
    }

    void debug(bool step, std::optional<std::size_t> run_to_line = std::nullopt) {
        if (!debugging()) {
            if (!confirm("Start debugging? Program statements may change DBF files."))
                return;
            reset_debugger();
            debug_output_.str({});
            debug_output_.clear();
            try {
                debug_program_ = std::make_unique<xabl::Program>(
                    xabl::Compiler{}.compile(document_.text()));
                debug_breakpoints_ = std::make_unique<xabl::BreakpointMap>(
                    *debug_program_);
                for (const auto line : breakpoint_lines_)
                    debug_breakpoints_->enable(line);
                debug_vm_ = std::make_unique<xabl::Vm>(debug_output_);
                const auto directory = document_.path().empty()
                    ? fs::current_path() : fs::absolute(document_.path()).parent_path();
                debug_vm_->begin(*debug_program_, directory);
            } catch (const std::exception& e) {
                reset_debugger();
                message_ = std::string("Debugger not started: ") + e.what();
                return;
            }
        }
        try {
            // A run-to-cursor breakpoint is temporary. Keep pre-existing
            // breakpoints enabled when the cursor is on one of them.
            bool temporary = false;
            if (run_to_line) {
                if (!debug_breakpoints_->instruction_at(*run_to_line)) {
                    message_ = "Cursor line has no executable statement.";
                    return;
                }
                temporary = debug_breakpoints_->enable(*run_to_line);
            }
            const auto reason = step ? debug_vm_->step()
                                     : debug_vm_->resume(*debug_breakpoints_);
            if (temporary) debug_breakpoints_->disable(*run_to_line);
            output_ = debug_output_.str();
            output_scroll_ = 0;
            if (reason == xabl::DebugStop::Finished) {
                reset_debugger();
                output_ += "\nProgram completed.\n";
                mode_ = Mode::Output;
                message_ = "Debugging completed. F6 for editor.";
                return;
            }
            const auto location = debug_vm_->debug_location();
            if (location) {
                (void)document_.go_to_line(location->line);
                mode_ = Mode::Editor;
                message_ = std::string(reason == xabl::DebugStop::Breakpoint
                              ? "Breakpoint before line " : "Stepped to line ")
                         + std::to_string(location->line)
                         + ". F7/F8 step; Ctrl+F9 continue; Ctrl+F2 stop.";
            } else {
                mode_ = Mode::Editor;
                message_ = "Debugger paused before unsourced bytecode.";
            }
        } catch (const std::exception& e) {
            output_ = debug_output_.str() + "\nError: " + e.what() + "\n";
            reset_debugger();
            mode_ = Mode::Output;
            message_ = "Debugging stopped after runtime error. F6 for editor.";
        }
    }

    void inspect_variables() {
        if (!debugging()) {
            message_ = "Start debugging to inspect variables.";
            return;
        }
        mode_ = mode_ == Mode::Variables ? Mode::Editor : Mode::Variables;
        output_scroll_ = 0;
        message_ = mode_ == Mode::Variables
            ? "Variables are read-only. Ctrl+F7 or Esc returns to the editor."
            : "Returned to editor.";
    }

    void stop_debugging() {
        if (!debugging()) {
            message_ = "No debugger session running.";
            return;
        }
        output_ = debug_output_.str();
        reset_debugger();
        mode_ = Mode::Editor;
        message_ = "Debugger stopped. Earlier database changes remain.";
    }

    static bool is_edit_key(int key) {
        return (key >= 32 && key <= 126) ||
            key == KEY_BACKSPACE || key == 127 || key == 8 ||
            key == KEY_DC || key == ctrl('Z') ||
            key == '\n' || key == '\r' || key == KEY_ENTER ||
            key == '\t';
    }

    void compile(bool execute) {
        // Compile without invoking the shell: the same library and dialect
        // configuration as the command-line XABL runner are used.
        std::ostringstream buffer;
        last_compile_error_line_.reset();
        bool compiled = false;
        try {
            xabl::Compiler compiler;
            const auto program = compiler.compile(document_.text());
            compiled = true;
            buffer << "Compilation successful.\n";
            if (execute) {
                xabl::Vm machine(buffer);
                const auto directory = document_.path().empty()
                    ? fs::current_path() : fs::absolute(document_.path()).parent_path();
                machine.run(program, directory);
                buffer << "\nProgram completed.\n";
            }
        } catch (const std::exception& e) {
            if (!compiled) {
                last_compile_error_line_ =
                    xabl::tui::diagnostic_source_line(e.what());
            }
            buffer << "\nError: " << e.what() << "\n";
        }
        output_ = buffer.str();
        output_scroll_ = 0;
        mode_ = Mode::Output;
        message_ = last_compile_error_line_
            ? "Syntax error at line " + std::to_string(*last_compile_error_line_) +
              ". F12 or Ctrl+E jumps there."
            : (execute ? "Program output. F6 to return." :
                         "Syntax check. F6 to return.");
    }

    void search(bool again) {
        if (!again) {
            last_search_ = prompt("Find text:");
            if (last_search_.empty()) return;
        }
        if (last_search_.empty()) {
            message_ = "Ctrl+F to enter search text.";
            return;
        }
        mode_ = Mode::Editor;
        message_ = document_.find_next(last_search_)
            ? "Found: " + last_search_ : "Not found: " + last_search_;
    }

    void run_or_continue() {
        if (debugging() || !breakpoint_lines_.empty()) {
            debug(false);
        } else if (confirm("Run source? It may write database files.")) {
            compile(true);
        }
    }

    void handle(int key) {
        if (key == ERR) return;
        if (mode_ == Mode::Browser) {
            if (key == KEY_F(10)) {
                browser_.reset();
                mode_ = Mode::Menu;
            } else if (key == key_exit || key == ctrl('Q')) {
                if (discard_ok()) active_ = false;
            } else {
                browse_key(key);
            }
            return;
        }
        if (mode_ == Mode::Menu) {
            mode_ = Mode::Editor;
            switch (std::tolower(static_cast<unsigned char>(key))) {
            case 's': handle(KEY_F(2)); break;
            case 'o': handle(KEY_F(3)); break;
            case 'p': handle(ctrl('O')); break;
            case 'n': handle(ctrl('N')); break;
            case 'c': handle(KEY_F(9)); break;
            case 'r': handle(key_run); break;
            case 'b': handle(key_breakpoint); break;
            case 't': handle(KEY_F(8)); break;
            case 'g': handle(KEY_F(4)); break;
            case 'v': handle(key_variables); break;
            case 'd': handle(key_reset); break;
            case 'u': handle(KEY_F(6)); break;
            case 'z': handle(KEY_F(5)); break;
            case 'h': handle(KEY_F(1)); break;
            case 'f': handle(ctrl('F')); break;
            case 'l': handle(ctrl('G')); break;
            case 'e': handle(ctrl('E')); break;
            case 'x': handle(key_exit); break;
            case 27: break;
            default: message_ = "Menu closed."; break;
            }
            return;
        }

        if (debugging() &&
            (key == KEY_F(2) || key == ctrl('S') ||
             key == KEY_F(3) || key == ctrl('O') || key == ctrl('N') ||
             key == KEY_F(9) || key == key_compile)) {
            message_ = "Ctrl+F2 resets debugging before changing or compiling source.";
            return;
        }
        switch (key) {
        case key_breakpoint: toggle_breakpoint(); return;
        case key_run: run_or_continue(); return;
        case key_reset: stop_debugging(); return;
        case key_variables: inspect_variables(); return;
        case key_output:
            mode_ = mode_ == Mode::Output ? Mode::Editor : Mode::Output;
            return;
        case key_compile: compile(false); return;
        case KEY_F(1):
            mode_ = mode_ == Mode::Help ? Mode::Editor : Mode::Help;
            output_scroll_ = 0;
            return;
        case KEY_F(2):
        case ctrl('S'): save(); return;
        case KEY_F(3): browse(); return;
        case ctrl('O'): open(); return;
        case ctrl('N'): new_document(); return;
        case KEY_F(4): debug(false, document_.cursor().row + 1); return;
        case KEY_F(5):
            zoomed_ = !zoomed_;
            mode_ = Mode::Editor;
            message_ = zoomed_ ? "Editor zoomed." : "Editor restored.";
            return;
        case KEY_F(6):
            mode_ = mode_ == Mode::Editor ? Mode::Output : Mode::Editor;
            return;
        case KEY_F(7):
        case KEY_F(8): debug(true); return;
        case ctrl('F'): search(false); return;
        case KEY_F(12):
        case ctrl('E'): next_or_error(); return;
        case ctrl('G'): goto_prompt(); return;
        case KEY_F(9): compile(false); return;
        case KEY_F(10):
            mode_ = mode_ == Mode::Menu ? Mode::Editor : Mode::Menu;
            output_scroll_ = 0;
            return;
        case key_exit:
        case ctrl('Q'):
            if (discard_ok()) active_ = false;
            return;
        case 27:
            mode_ = Mode::Editor;
            return;
        case KEY_RESIZE: return;
        default: break;
        }
        if (mode_ != Mode::Editor) {
            switch (key) {
            case KEY_UP: if (output_scroll_) --output_scroll_; break;
            case KEY_DOWN: ++output_scroll_; break;
            case KEY_PPAGE:
                output_scroll_ = output_scroll_ > 12 ? output_scroll_ - 12 : 0;
                break;
            case KEY_NPAGE: output_scroll_ += 12; break;
            default: break;
            }
            return;
        }
        const bool editing = is_edit_key(key);
        if (editing && debugging()) {
            message_ = "Editor paused: Ctrl+F2 to reset before editing.";
            return;
        }
        const bool track_source = editing && !breakpoint_lines_.empty();
        const std::string prior_source = track_source ? document_.text() : "";
        switch (key) {
        case KEY_UP: document_.move_up(); break;
        case KEY_DOWN:
        case ctrl('X'): document_.move_down(); break; // WordStar cursor keys
        case KEY_LEFT: document_.move_left(); break;
        case KEY_RIGHT:
        case ctrl('D'): document_.move_right(); break;
        case KEY_HOME: document_.home(); break;
        case KEY_END: document_.end(); break;
        case KEY_PPAGE:
            for (int i = 0; i < 12; ++i) document_.move_up();
            break;
        case KEY_NPAGE:
            for (int i = 0; i < 12; ++i) document_.move_down();
            break;
        case KEY_BACKSPACE:
        case 127:
        case 8: document_.backspace(); break;
        case KEY_DC: document_.erase(); break;
        case ctrl('Z'):
            message_ = document_.undo() ? "Undid last change." : "Nothing to undo.";
            break;
        case '\n':
        case '\r':
        case KEY_ENTER: document_.newline(); break;
        case '\t': document_.insert_text("    "); break;
        default:
            if (key >= 32 && key <= 126)
                document_.insert(static_cast<char>(key));
            break;
        }
        if (track_source && document_.text() != prior_source) {
            breakpoint_lines_.clear();
            message_ = "Source changed; breakpoints cleared.";
        }
    }

};

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: xabl-tui [program.prg|program.xabl]\n"
                  << "Terminal IDE (optional ncurses build).\n"
                  << "F1 Help, F2 Save, F3 Open, F4 Run to cursor, F5 Zoom, F6 Output,\n"
                  << "F7 Trace, F8 Step, F9 Build, F10 Menu, Alt+X Exit.\n"
                  << "Ctrl+F8 Breakpoint, Ctrl+F9 Run, Ctrl+F2 Reset; F10 menu fallback.\n";
        return 0;
    }
    if (argc > 2) {
        std::cerr << "Usage: xabl-tui [program.prg|program.xabl]\n";
        return 2;
    }
    std::setlocale(LC_ALL, "");
    try {
        TextStudio studio(argc == 2 ? fs::path(argv[1]) : fs::path{});
        return studio.run();
    } catch (const std::exception& e) {
        std::cerr << "xabl-tui: " << e.what() << "\n";
        return 1;
    }
}
