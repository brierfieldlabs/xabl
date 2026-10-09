# ADR: Remove source comments before line-command parsing

Status: Accepted, 9 October 2026

dBASE III PLUS programs support full-line * and NOTE comments as well as
&& comments, which may appear after executable statements. The initial
line parser recognised only * comments, rejecting ordinary annotated PRGs.

Strip && comments before dispatching a statement. The small quote-aware
scanner honours both single and double quotes and doubled-delimiter
escaping, so strings containing && are preserved. It also recognises an
entire NOTE line and a line beginning with &&. Blank lines are ignored.

The ? output command now permits immediately adjacent expressions
(?LEN(NAME)) as well as spaced forms, and a bare ? emits a blank line.
Original line numbers remain in diagnostics after comment removal.
Control-flow markers may have inline comments.

The scanner currently handles single physical lines only. Semicolon
continuation, nested multiline construction, macro expansion and
version-specific bare ENDIF/ENDDO comment exceptions still require
explicit dialect tests. Do not silently enable later-source dialect
features before compatibility profiles support them.

References: dBASE Language Handbook, '&&'; Programming With dBASE III PLUS,
Computer History Museum (chapter 1).
https://www.terrellamedia.com/wp-content/uploads/2022/01/dBASE-Language-Handbook-by-David-M-Kalman-Final.pdf
https://archive.computerhistory.org/resources/access/text/2024/05/102734488-05-0003-acc.pdf
