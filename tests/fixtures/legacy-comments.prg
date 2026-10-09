* Traditional dBASE III PLUS comment and compact-output fixture.
NOTE This entire line is a program note.
&& A double ampersand can also start a whole-line comment.
?100 && a compact print expression
? "two&&three" && comment after a quoted ampersand
? 'Don''t && quit' && doubled apostrophe keeps the string quoted
? 2+3 && no mandatory blank after command marker
? && print an empty line
IF .T. && preserve IF scope
    ? UPPER('ok') && ordinary inline comment
ELSE && unexecuted branch
    ? "WRONG" && should be skipped
ENDIF && close block
STORE "A&&B" TO LABEL && quotation must remain intact
?LABEL && immediate variable print
STORE "A TO B" TO MESSAGE && TO inside a quoted value is not a separator
? MESSAGE
STORE UPPER("ready TO go") TO MESSAGE && TO inside a nested function
? MESSAGE
STORE 'A'' TO ''B' TO MESSAGE && doubled quote escaping
? MESSAGE
USE customers && open a table
SET FILTER TO LEFT(NAME,1) == 'C' && filter expression
GO TOP && move in view
?TRIM(NAME)&& even with no whitespace before comment
USE && close the work area
