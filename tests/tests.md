# Tests

This file serves as a repository for testing out the program's various capabilities. The host domain is nforce.com, and obviously, the validity of these links here are subject to them.

### 10 MB file

Please use this link here: https://mirror.nforce.com/pub/speedtests/10mb.bin
As per the current program, it should check whether threads are being created on 10 MB files (due to the >50 MB requirement).
Ideally, it should not.

### 50 MB file

Please use this link here: https://mirror.nforce.com/pub/speedtests/50mb.bin
Given the >50 MB limit, this one here is for edge case testing.
Ideally, it should still create threads.

### 100 MB file

Please use this link here: https://mirror.nforce.com/pub/speedtests/100mb.bin
This satisfies the >50 MB limit, and ideally, this link should force multi-threading behavior.
