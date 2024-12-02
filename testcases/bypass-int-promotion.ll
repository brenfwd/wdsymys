define i32 @main() {
entry:
    ; Declare and initialize constants
    %a = add i8 0, 0
    %b = add i8 0, 1
    %c = add i16 0, 2
    %d = add i32 0, 3

    ; Add two i8 values, extend to i16
    %ab_i8 = add i8 %a, %b
    %ab = zext i8 %ab_i8 to i16

    ; Add i16 value to the extended i8 result
    %abc = add i16 %ab, %c

    ; Extend the i16 result to i32 and add to %d
    %zext = zext i16 %abc to i32
    %abcd = add i32 %d, %zext

    ; Do nothing with %abcd (just for demonstration)
    ret i32 0
}