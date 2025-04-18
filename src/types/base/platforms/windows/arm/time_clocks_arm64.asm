    EXPORT Time_clocksAsm
    AREA |.text|, CODE, READONLY
    ALIGN 4
Time_clocksAsm PROC
    mrs x0, cntvct_el0
    ret
Time_clocksAsm ENDP
    END