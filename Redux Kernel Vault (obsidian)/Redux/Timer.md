The timer in Redux is APIC based. It uses a One Shot APIC as the Timer and ACPI PM for calibration.
It is used in lots of things, some not implemented yet, but some examples are:
Cursor blinking
Multitasking
and Sleep function for the API

This was added in the commit Timer, clock and ACPI stuff.
### One-Shot Mode

For one-shot mode, the local APIC decrements the current count (and generates a timer IRQ when the count reaches zero) in the same way as in periodic mode; however it doesn't reset the current count to the initial count when the current count reaches zero. Instead, software has to set a new count each time if it wants more timer IRQs.

The advantage of this mode is that software can have precise control over when timer IRQs occur. For example, during task switches an OS could set the count to a value that depends on the new task's priority (so that some tasks run for a small amount of time and other tasks run for a larger amount of time), and there wouldn't be any unwanted IRQs. Some OSs use this approach to implement a generic high precision timer service, where the local APIC count is set to a value that depends on which event will happen soonest. For example, if the currently running task switch should be pre-empted in 1234 nanoseconds, a sleeping task needs to wake up in 333 nanoseconds and alarm signal has to be sent in 44444 nanoseconds, then the timer's count would be set to 333 nanoseconds (the earliest delay needed) and when the the timer IRQ occurs the OS knows that there's 901 nanoseconds remaining before the current task should be pre-empted and 441111 nanoseconds until the alarm signal needs to be sent (and would set the count to 901 nanoseconds for the next timer IRQ).

The disadvantages are that it's harder to track real-time with one-shot mode and special care needs to be taken to avoid race conditions; especially if a new count is set before the old count expires.

## Are more time modes planned?
Of course, but at the moment, its better to get it working and done than add everything at once!
