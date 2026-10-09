// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
// Keyboard-first terminal IDE. ncurses is an optional frontend only;
// parsing, bytecode and execution remain in the shared xabl_core library.
#include "editor_buffer.hpp"

#include <xabl/runtime/xabl.hpp>

#include <curses.h>
#include <algorithm>
#include <array>
#include <clocale>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace {
class CursesSession {
public:
    CursesSession() {
        if (!initscr()) throw std::runtime_error("cannot initialise terminal");
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(1);
#if defined(__unix__) || defined(__APPLE__)
        // Ctrl+S / Ctrl+Q are IDE shortcuts. IXON would otherwise treat
        // them as terminal XOFF/XON and freeze the visible screen.
        // endwin() restores the terminal attributes saved by initscr().
        termios mode{};
        if (tcgetattr(STDIN_FILENO, &mode) == 0) {
            mode.c_iflag &= static_cast<tcflag_t>(~(IXON | IXOFF));
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
    enum class Mode { Editor, Output, Help };
    xabl::tui::EditorBuffer document_;
    Mode mode_{Mode::Editor};
    std::string message_;
    std::string output_;
    std::string last_search_;
    std::size_t scroll_row_{};
    std::size_t scroll_column_{};
    std::size_t output_scroll_{};
    bool active_{true};

    static void paint(int y, int x, std::string_view text, int width) {
        if (y < 0 || x < 0 || width <= 0) return;
        const std::string clipped(text.substr(0, static_cast<std::size_t>(width)));
        mvaddnstr(y, x, clipped.c_str(), width);
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
            paint(height - 1, 1, "F10 Exit", width - 2);
            refresh();
            return;
        }

        std::string heading = " XABL Text Studio  |  ";
        heading += mode_ == Mode::Editor ? "EDITOR" :
                   mode_ == Mode::Output ? "PROGRAM OUTPUT" : "HELP";
        bar(0, heading, 2);

        const std::string filename = document_.path().empty()
            ? "UNTITLED.PRG" : document_.path().string();
        bar(1, " File: " + filename + (document_.dirty() ? "  [Modified]" : ""), 1);

        const int viewport_rows = height - 5;
        const int viewport_width = width - gutter_width - 1;
        attrset(COLOR_PAIR(mode_ == Mode::Output ? 4 : 1));

        if (mode_ == Mode::Editor) {
            ensure_visible(viewport_rows, viewport_width);
            for (int visible = 0; visible < viewport_rows; ++visible) {
                const std::size_t index = scroll_row_ + visible;
                if (index >= document_.lines().size()) break;
                std::ostringstream label;
                label.width(4);
                label << (index + 1) << " |";
                attron(A_DIM);
                paint(editor_top + visible, 0, label.str(), gutter_width);
                attroff(A_DIM);
                const auto& source = document_.lines()[index];
                if (scroll_column_ < source.size())
                    paint(editor_top + visible, gutter_width,
                          std::string_view(source).substr(scroll_column_),
                          viewport_width);
            }
        } else {
            std::vector<std::string> rows;
            std::istringstream stream(mode_ == Mode::Output ? output_ : help_text());
            std::string line;
            while (std::getline(stream, line)) rows.push_back(line);
            output_scroll_ = std::min(output_scroll_, rows.size());
            for (int visible = 0; visible < viewport_rows; ++visible) {
                const auto row = output_scroll_ + static_cast<std::size_t>(visible);
                if (row < rows.size())
                    paint(editor_top + visible, 2, rows[row], width - 4);
            }
        }

        std::ostringstream status;
        status << " " << (mode_ == Mode::Editor ? "Edit" :
                           mode_ == Mode::Output ? "Output" : "Help")
               << "  Line " << document_.cursor().row + 1
               << "  Col " << document_.cursor().column + 1
               << "  " << document_.lines().size() << " lines"
               << (document_.dirty() ? "  * UNSAVED *" : "");
        bar(height - 3, status.str(), 2);
        bar(height - 2, " " + message_, 1);
        bar(height - 1,
            " F1 Help  F2 Open  F3 New  F4 Save  F5 Run  F6 Output  F7 Find  F9 Check  F10 Exit",
            2);
        if (mode_ == Mode::Editor) {
            curs_set(1);
            const auto cursor = document_.cursor();
            move(static_cast<int>(cursor.row - scroll_row_) + editor_top,
                 static_cast<int>(cursor.column - scroll_column_) + gutter_width);
        } else {
            curs_set(0);
            move(height - 2, 1);
        }
        refresh();
    }

    static std::string help_text() {
        return
            "XABL TEXT STUDIO   |   DOS-inspired keyboard IDE\n"
            "\n"
            " F1       Toggle this help screen\n"
            " F2       Open an existing .prg / .xabl source file\n"
            " F3       New source file (asks before discarding changes)\n"
            " F4       Save source file (Ctrl+S also works)\n"
            " F5       Compile and run the source in shared XABL VM\n"
            " F6       Toggle the program output / editor screen\n"
            " F7       Find text (Ctrl+F), F8 find next\n"
            " F9       Check syntax without executing any commands\n"
            " F10      Exit (Ctrl+Q also works)\n"
            " Ctrl+Z   Undo recent edit\n"
            " Arrows   Navigate; Home/End and PgUp/PgDn supported\n"
            " Enter    New line; Tab inserts four spaces\n"
            " Esc      Return to the editor\n"
            "\n"
            "WARNING: F5 runs your program. DBF commands may write data.\n"
            "F9 is compile-only, suitable for checking code safely.\n"
            "\n"
            "Current scope: ASCII-oriented terminal editing, <= 1 MiB.\n"
            "Files preserve CRLF when opened from DOS/Windows sources.\n"
            "No DOS emulation or native DOS binary is implied.\n";
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
        message_ = "Opened " + path;
    }

    void new_document() {
        if (!discard_ok()) return;
        document_.clear();
        scroll_row_ = scroll_column_ = 0;
        mode_ = Mode::Editor;
        message_ = "New source file.";
    }

    void compile(bool execute) {
        // Compile without invoking the shell: the same library and dialect
        // configuration as the command-line XABL runner are used.
        std::ostringstream buffer;
        try {
            xabl::Compiler compiler;
            const auto program = compiler.compile(document_.text());
            buffer << "Compilation successful.\n";
            if (execute) {
                xabl::Vm machine(buffer);
                const auto directory = document_.path().empty()
                    ? fs::current_path() : fs::absolute(document_.path()).parent_path();
                machine.run(program, directory);
                buffer << "\nProgram completed.\n";
            }
        } catch (const std::exception& e) {
            buffer << "\nError: " << e.what() << "\n";
        }
        output_ = buffer.str();
        output_scroll_ = 0;
        mode_ = Mode::Output;
        message_ = execute ? "Program output. F6 to return." :
                             "Syntax check. F6 to return.";
    }

    void search(bool again) {
        if (!again) {
            last_search_ = prompt("Find text:");
            if (last_search_.empty()) return;
        }
        if (last_search_.empty()) {
            message_ = "F7 to enter search text.";
            return;
        }
        mode_ = Mode::Editor;
        message_ = document_.find_next(last_search_)
            ? "Found: " + last_search_ : "Not found: " + last_search_;
    }

    void handle(int key) {
        if (key == ERR) return;
        switch (key) {
        case KEY_F(1):
            mode_ = mode_ == Mode::Help ? Mode::Editor : Mode::Help;
            output_scroll_ = 0;
            return;
        case KEY_F(2): open(); return;
        case KEY_F(3): new_document(); return;
        case KEY_F(4):
        case ctrl('S'): save(); return;
        case KEY_F(5):
            if (confirm("Run source? It may write database files.")) compile(true);
            return;
        case KEY_F(6):
            mode_ = mode_ == Mode::Editor ? Mode::Output : Mode::Editor;
            return;
        case KEY_F(7):
        case ctrl('F'): search(false); return;
        case KEY_F(8): search(true); return;
        case KEY_F(9): compile(false); return;
        case KEY_F(10):
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
        switch (key) {
        case KEY_UP: document_.move_up(); break;
        case KEY_DOWN: document_.move_down(); break;
        case KEY_LEFT: document_.move_left(); break;
        case KEY_RIGHT: document_.move_right(); break;
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
    }
};

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: xabl-tui [program.prg|program.xabl]\n"
                  << "Terminal IDE (optional ncurses build).\n"
                  << "F1 Help, F2 Open, F3 New, F4 Save, F5 Run, F6 Output, F9 Check, F10 Exit.\n";
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
