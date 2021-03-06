//
//  Copyright (c) 2015  Finnbarr P. Murphy.   All rights reserved.
//
//  Display core EDID v1.3 information. 
//
//  License: BSD License
//

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/ShellLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>

#include <Protocol/EfiShell.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/EdidActive.h>
#include <Protocol/EdidDiscovered.h>


typedef struct {
    UINT8  Header[8];                        //EDID header "00 FF FF FF FF FF FF 00"
    UINT16 ManufactureName;                  //EISA 3-character ID
    UINT16 ProductCode;                      //Vendor assigned code
    UINT32 SerialNumber;                     //32-bit serial number
    UINT8  WeekOfManufacture;                //Week number
    UINT8  YearOfManufacture;                //Year
    UINT8  EdidVersion;                      //EDID Structure Version
    UINT8  EdidRevision;                     //EDID Structure Revision
    UINT8  VideoInputDefinition;
    UINT8  MaxHorizontalImageSize;           //cm
    UINT8  MaxVerticalImageSize;             //cm
    UINT8  DisplayGamma;
    UINT8  DpmSupport;
    UINT8  RedGreenLowBits;                  //Rx1 Rx0 Ry1 Ry0 Gx1 Gx0 Gy1Gy0
    UINT8  BlueWhiteLowBits;                 //Bx1 Bx0 By1 By0 Wx1 Wx0 Wy1 Wy0
    UINT8  RedX;                             //Red-x Bits 9 - 2
    UINT8  RedY;                             //Red-y Bits 9 - 2
    UINT8  GreenX;                           //Green-x Bits 9 - 2
    UINT8  GreenY;                           //Green-y Bits 9 - 2
    UINT8  BlueX;                            //Blue-x Bits 9 - 2
    UINT8  BlueY;                            //Blue-y Bits 9 - 2
    UINT8  WhiteX;                           //White-x Bits 9 - 2
    UINT8  WhiteY;                           //White-x Bits 9 - 2
    UINT8  EstablishedTimings[3];
    UINT8  StandardTimingIdentification[16];
    UINT8  DescriptionBlock1[18];
    UINT8  DescriptionBlock2[18];
    UINT8  DescriptionBlock3[18];
    UINT8  DescriptionBlock4[18];
    UINT8  ExtensionFlag;                    //Number of (optional) 128-byte EDID extension blocks
    UINT8  Checksum;
} __attribute__((__packed__)) EDID_DATA_BLOCK;


// most of these defines come straight out of NetBSD EDID source code
#define CHECK_BIT(var, pos)               ((var & (1 << pos)) == (1 << pos))
#define EDID_COMBINE_HI_8LO(hi, lo)       ((((unsigned)hi) << 8) | (unsigned)lo)
#define EDID_DATA_BLOCK_SIZE              0x80
#define EDID_VIDEO_INPUT_LEVEL(x)         (((x) & 0x60) >> 5)
#define EDID_DPMS_ACTIVE_OFF              (1 << 5)
#define EDID_DPMS_SUSPEND                 (1 << 6)
#define EDID_DPMS_STANDBY                 (1 << 7)
#define EDID_STD_TIMING_HRES(ptr)         ((((ptr)[0]) * 8) + 248)
#define EDID_STD_TIMING_VFREQ(ptr)        ((((ptr)[1]) & 0x3f) + 60)
#define EDID_STD_TIMING_RATIO(ptr)        ((ptr)[1] & 0xc0)
#define EDID_BLOCK_IS_DET_TIMING(ptr)     ((ptr)[0] | (ptr)[1])
#define EDID_DET_TIMING_DOT_CLOCK(ptr)    (((ptr)[0] | ((ptr)[1] << 8)) * 10000)
#define EDID_HACT_LO(ptr)                 ((ptr)[2])
#define EDID_HBLK_LO(ptr)                 ((ptr)[3])
#define EDID_HACT_HI(ptr)                 (((ptr)[4] & 0xf0) << 4)
#define EDID_HBLK_HI(ptr)                 (((ptr)[4] & 0x0f) << 8)
#define EDID_DET_TIMING_HACTIVE(ptr)      (EDID_HACT_LO(ptr) | EDID_HACT_HI(ptr))
#define EDID_DET_TIMING_HBLANK(ptr)       (EDID_HBLK_LO(ptr) | EDID_HBLK_HI(ptr))
#define EDID_VACT_LO(ptr)                 ((ptr)[5])
#define EDID_VBLK_LO(ptr)                 ((ptr)[6])
#define EDID_VACT_HI(ptr)                 (((ptr)[7] & 0xf0) << 4)
#define EDID_VBLK_HI(ptr)                 (((ptr)[7] & 0x0f) << 8)
#define EDID_DET_TIMING_VACTIVE(ptr)      (EDID_VACT_LO(ptr) | EDID_VACT_HI(ptr))
#define EDID_DET_TIMING_VBLANK(ptr)       (EDID_VBLK_LO(ptr) | EDID_VBLK_HI(ptr))
#define EDID_HOFF_LO(ptr)                 ((ptr)[8])
#define EDID_HWID_LO(ptr)                 ((ptr)[9])
#define EDID_VOFF_LO(ptr)                 ((ptr)[10] >> 4)
#define EDID_VWID_LO(ptr)                 ((ptr)[10] & 0xf)
#define EDID_HOFF_HI(ptr)                 (((ptr)[11] & 0xc0) << 2)
#define EDID_HWID_HI(ptr)                 (((ptr)[11] & 0x30) << 4)
#define EDID_VOFF_HI(ptr)                 (((ptr)[11] & 0x0c) << 2)
#define EDID_VWID_HI(ptr)                 (((ptr)[11] & 0x03) << 4)
#define EDID_DET_TIMING_HSYNC_OFFSET(ptr) (EDID_HOFF_LO(ptr) | EDID_HOFF_HI(ptr))
#define EDID_DET_TIMING_HSYNC_WIDTH(ptr)  (EDID_HWID_LO(ptr) | EDID_HWID_HI(ptr))
#define EDID_DET_TIMING_VSYNC_OFFSET(ptr) (EDID_VOFF_LO(ptr) | EDID_VOFF_HI(ptr))
#define EDID_DET_TIMING_VSYNC_WIDTH(ptr)  (EDID_VWID_LO(ptr) | EDID_VWID_HI(ptr))
#define EDID_HSZ_LO(ptr)                  ((ptr)[12])
#define EDID_VSZ_LO(ptr)                  ((ptr)[13])
#define EDID_HSZ_HI(ptr)                  (((ptr)[14] & 0xf0) << 4)
#define EDID_VSZ_HI(ptr)                  (((ptr)[14] & 0x0f) << 8)
#define EDID_DET_TIMING_HSIZE(ptr)        (EDID_HSZ_LO(ptr) | EDID_HSZ_HI(ptr))
#define EDID_DET_TIMING_VSIZE(ptr)        (EDID_VSZ_LO(ptr) | EDID_VSZ_HI(ptr))
#define EDID_DET_TIMING_HBORDER(ptr)      ((ptr)[15])
#define EDID_DET_TIMING_VBORDER(ptr)      ((ptr)[16])
#define EDID_DET_TIMING_FLAGS(ptr)        ((ptr)[17])
#define EDID_DET_TIMING_VSOBVHSPW(ptr)    ((ptr)[11])


//
// Based on code found at http://code.google.com/p/my-itoa/
//
int 
Integer2AsciiString(int val, char* buf)
{
    const unsigned int radix = 10;

    char* p = buf;
    unsigned int a; 
    int len;
    char* b;
    char temp;
    unsigned int u;

    if (val < 0) {
        *p++ = '-';
        val = 0 - val;
    }
    u = (unsigned int)val;
    b = p;

    do {
        a = u % radix;
        u /= radix;
        *p++ = a + '0';
    } while (u > 0);

    len = (int)(p - buf);
    *p-- = 0;

    // swap 
    do {
       temp = *p; *p = *b; *b = temp;
       --p; ++b;
    } while (b < p);

    return len;
}


//
// Based on code found on the Internet (author unknown)
// Search for ftoa implementations
//
int 
Float2AsciiString(float f, char *buffer, int numdecimals)
{
    int status = 0;
    char *s = buffer;
    long mantissa, int_part, frac_part;
    short exp2;
    char m;

    typedef union {
        long L;
        float F;
    } LF_t;
    LF_t x;

    if (f == 0.0) {           // return 0.00
        *s++ = '0'; *s++ = '.'; *s++ = '0'; *s++ = '0'; 
        *s = 0;
       return status;
    }

    x.F = f;

    exp2 = (unsigned char)(x.L >> 23) - 127;
    mantissa = (x.L & 0xFFFFFF) | 0x800000;
    frac_part = 0;
    int_part = 0;

    if (exp2 >= 31 || exp2 < -23) {
        *s = 0;
        return 1;
    } 

    if (exp2 >= 0) {
        int_part = mantissa >> (23 - exp2);
        frac_part = (mantissa << (exp2 + 1)) & 0xFFFFFF;
    } else {
        frac_part = (mantissa & 0xFFFFFF) >> -(exp2 + 1);
    }

    if (int_part == 0)
       *s++ = '0';
    else {
        Integer2AsciiString(int_part, s);
        while (*s) s++;
    }
    *s++ = '.';

    if (frac_part == 0)
        *s++ = '0';
    else {
        for (m = 0; m < numdecimals; m++) {                       // print BCD
            frac_part = (frac_part << 3) + (frac_part << 1);      // frac_part *= 10
            *s++ = (frac_part >> 24) + '0';
            frac_part &= 0xFFFFFF;
        }
    }
    *s = 0;

    return status;
}


VOID
Ascii2UnicodeString(CHAR8 *String, CHAR16 *UniString)
{
    while (*String != '\0') {
        *(UniString++) = (CHAR16) *(String++);
    }
    *UniString = '\0';
}


CHAR16 *
DisplayGammaString(UINT8 Gamma)
{
    char Str[8];
    static CHAR16 Wstr[8];

    float g1 = (float)Gamma;
    float g2 = 1.00 + (g1/100);

    Float2AsciiString(g2, Str, 2);         
    Ascii2UnicodeString(Str, Wstr);

    return Wstr;
}


CHAR16 *
ManufacturerAbbrev(UINT16 *ManufactureName)
{
    static CHAR16 Mcode[8];
    UINT8 *block = (UINT8 *)ManufactureName;
    UINT16 h = EDID_COMBINE_HI_8LO(block[0], block[1]);

    Mcode[0] = (CHAR16)((h>>10) & 0x1f) + 'A' - 1;
    Mcode[1] = (CHAR16)((h>>5) & 0x1f) + 'A' - 1;
    Mcode[2] = (CHAR16)(h & 0x1f) + 'A' - 1;
    Mcode[3] = (CHAR16)'\0';
 
    return Mcode;
}


int
CheckForValidEdid(EDID_DATA_BLOCK *EdidDataBlock)
{
    const UINT8 EdidHeader[] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
    UINT8 *edid = (UINT8 *)EdidDataBlock;
    UINT8 i;
    UINT8 CheckSum = 0;

    for (i = 0; i < EDID_DATA_BLOCK_SIZE; i++) {
        CheckSum += edid[i];
    }
    if (CheckSum != 0) {
        return(1);
    }
 
    if (*edid == 0x00) {
       CheckSum = 0;
       for (i = 0; i < 8; i++) {
           if (*edid++ == EdidHeader[i])
               CheckSum++;
       }
       if (CheckSum != 8) {
           return(1);
       }
    }
 
    if (EdidDataBlock->EdidVersion != 1 || EdidDataBlock->EdidRevision > 4) {
        return(1);
    }
 
    return(0);
}


VOID
PrintDetailedTimingBlock(UINT8 *dtb)
{
    Print(L"Horizonal Image Size: %d mm\n", EDID_DET_TIMING_HSIZE(dtb));
    Print(L" Vertical Image Size: %d mm\n", EDID_DET_TIMING_VSIZE(dtb));
    Print(L"HoriImgSzByVertImgSz: %d\n", dtb[14]);
    Print(L"   Horizontal Border: %d\n", EDID_DET_TIMING_HBORDER(dtb));
    Print(L"     Vertical Border: %d\n", EDID_DET_TIMING_VBORDER(dtb));
}


VOID
PrintEdid( EDID_DATA_BLOCK *EdidDataBlock)
{ 
    UINT8 tmp;

    Print(L"        EDID Version: %d\n", EdidDataBlock->EdidVersion);
    Print(L"       EDID Revision: %d\n", EdidDataBlock->EdidRevision);
    Print(L" Vendor Abbreviation: %s\n", ManufacturerAbbrev(&(EdidDataBlock->ManufactureName)));
    Print(L"          Product ID: %08X\n", EdidDataBlock->ProductCode);
    Print(L"       Serial Number: %08X\n", EdidDataBlock->SerialNumber);
    Print(L"    Manufacture Week: %02d\n", EdidDataBlock->WeekOfManufacture);
    Print(L"    Manufacture Year: %d\n", EdidDataBlock->YearOfManufacture + 1990);

    tmp = (UINT8) EdidDataBlock->VideoInputDefinition;
    Print(L"         Video Input: ");
    if (CHECK_BIT(tmp, 7)) {
        Print(L"Analog\n");
    } else {
        Print(L"Digital\n");
    }
    if (tmp & 0x1F) {
        Print(L"      Syncronization: ");
        if (CHECK_BIT(tmp, 4))
            Print(L"BlankToBackSetup ");
        if (CHECK_BIT(tmp, 3))
            Print(L"SeparateSync ");
        if (CHECK_BIT(tmp, 2))
            Print(L"CompositeSync ");
        if (CHECK_BIT(tmp, 1))
            Print(L"SyncOnGreen ");
        if (CHECK_BIT(tmp, 0))
            Print(L"SerrationVSync ");
        Print(L"\n");
    }

    tmp = (UINT8) EdidDataBlock->DpmSupport;
    Print(L"        Display Type: ");
    if (CHECK_BIT(tmp, 3) && CHECK_BIT(tmp, 4)) {
        Print(L"Undefined");
    } else if (CHECK_BIT(tmp, 3)) {
        Print(L"RGB color");
    } else if (CHECK_BIT(tmp, 4)) {
        Print(L"Non-RGB multicolor");
    } else {
        Print(L"Monochrome");
    }
    Print(L"\n");

    Print(L"  Max Horizonal Size: %1d cm\n", EdidDataBlock->MaxHorizontalImageSize);
    Print(L"   Max Vertical Size: %1d cm\n", EdidDataBlock->MaxVerticalImageSize);
    Print(L"               Gamma: %s\n", DisplayGammaString(EdidDataBlock->DisplayGamma));

    PrintDetailedTimingBlock((UINT8 *)&(EdidDataBlock->DescriptionBlock1[0]));
}


INTN
EFIAPI
ShellAppMain(UINTN Argc, CHAR16 **Argv)
{
    EFI_STATUS Status = EFI_SUCCESS;

    EFI_GUID gEfiEdidDiscoveredProtocolGuid = EFI_EDID_DISCOVERED_PROTOCOL_GUID;
    EFI_EDID_DISCOVERED_PROTOCOL *Edp;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount = 0;
    BOOLEAN Found = FALSE;

    // Try locating GOP by handle
    Status = gBS->LocateHandleBuffer( ByProtocol,
                                      &gEfiGraphicsOutputProtocolGuid,
                                      NULL,
                                      &HandleCount,
                                      &HandleBuffer);
    if (EFI_ERROR (Status)) {
        Print(L"ERROR: No GOP handle found. Cannot locate an EDID.\n");
        return EFI_SUCCESS;
    }

    for (int i = 0; i < HandleCount; i++) {
        Status = gBS->OpenProtocol( HandleBuffer[i],
                                    &gEfiEdidDiscoveredProtocolGuid, 
                                    (VOID **)&Edp,
                                    gImageHandle,
                                    NULL,
                                    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
        if (Status == EFI_SUCCESS) {
            if (!CheckForValidEdid((EDID_DATA_BLOCK *)(Edp->Edid))) { 
                Found = TRUE;
                PrintEdid((EDID_DATA_BLOCK *)(Edp->Edid));
            } else {
                Print(L"ERROR: Invalid EDID checksum\n");
            }
        }
    }

    if (!Found) {
        Print(L"Cannot locate an EDID.\n");
    }

    return EFI_SUCCESS;
}
