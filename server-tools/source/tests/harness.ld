SECTIONS {
 .lookup 0xf4370 : { BYTE(0xe9) LONG(lookup - . - 4) }
 .allocmessage 0xf88c0 : { BYTE(0xe9) LONG(alloc_message - . - 4) }
 .broadcast 0xf8c40 : { BYTE(0xe9) LONG(broadcast - . - 4) }
 .originalchat 0xf8e90 : { BYTE(0xe9) LONG(original_chat - . - 4) }
 .forward 0xf96f0 : { BYTE(0xe9) LONG(forward - . - 4) }
 .text 0x116580 : { *(.text) }
 .iat 0x117000 : { *(.iat) }
 .fakeexe 0x200000 : { *(.fakeexe) }
 .harness 0x400000 : { *(.harness) }
 .testdata 0x500000 : { *(.testdata) }
}
