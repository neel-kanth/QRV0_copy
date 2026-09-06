# QM35825 Software Development Kit

Welcome to the official Qorvo® IoT Software Development Kit (SDK) for the QM35825 System-on-Chip.

This SDK provides everything you need to develop Ultra-Wideband (UWB) applications with the QM35825, including documentation, examples, tools, and platform support.

## Getting the SDK

### Option 1: Git Clone (Recommended)
```bash
git clone https://gitlab.com/qorvo_sdk/public/devkits/qm35-sdk.git
cd qm35-sdk
```

### Option 2: Download ZIP Archive
1. Go to the [project page](https://gitlab.com/qorvo_sdk/public/devkits/qm35-sdk)
2. Click the **Code** button (cloud icon)
3. Select **zip** in **Download source code section**
4. Extract the archive to your desired location

## Getting Started

1. **Learn about QM35825**: Read the [Quick Start Guide](.//Quick_Start_Guide.pdf) and explore [QM35 products](https://www.qorvo.com/search?mode=2&key=&des=QM35&exact=&any=&none=&pageNumber=1&pageSize=10&tbl=ta0132) on Qorvo.com
2. **Follow the tutorial**: Start with the [QM35 DK-05 User Manual](./Documentation/Guides/QM35_DK-05_User_Manual.pdf)

For Hardware information, read [Hardware User Manual](./Documentation/Guides/QM35_DK-05_HW_User_Manual.pdf).

## SDK Structure

This SDK provides comprehensive tools and documentation for QM35825 development:

- **[Application/](./Application/)** - Ready-to-use applications including Explorer GUI
- **[Binaries/](./Binaries/)** - Firmware binaries for QM35825
- **[Documentation/](./Documentation/)** - Complete documentation set including guides and specifications
- **[Platform/](./Platform/)** - Platform-specific implementations (Linux drivers, adaptation for nRF52840 with Zephyr)
- **[Samples/](./Samples/)** - Example code and tools (Python, Cherry framework)
- **[Tools/](./Tools/)** - Development and flashing utilities

## Additional Resources

### Documentation

- [DK-06 User Manual](./Documentation/Guides/QM35_DK-06_User_Manual.pdf) - Adaptation of DK-05 for nRF52840 host with Zephyr-OS
- [Release Notes](./Release_Notes.pdf) - Latest changes and known issues
- [Software License Agreement](./Documentation/Qorvo_Software_License_Agreement.pdf) - Terms and conditions for SDK usage

### Support
- Visit [www.qorvo.com](https://www.qorvo.com) for technical support and additional resources
- Browse [QM35 products](https://www.qorvo.com/search?mode=2&key=&des=QM35&exact=&any=&none=&pageNumber=1&pageSize=10&tbl=ta0132) on Qorvo.com

---

## Regulatory Information

### FCC Notice

This development kit is designed to allow:

1. **Product developers** to evaluate electronic components, circuitry, or software associated with the kit to determine whether to incorporate such items in a finished product.

2. **Software developers** to write software applications for use with the end product.

**Important**: This kit is not a finished product and when assembled may not be resold or otherwise marketed unless all required FCC equipment authorizations are first obtained.

Operation is subject to the condition that this product not cause harmful interference to licensed radio stations and that this product accept harmful interference. Unless the assembled kit is designed to operate under part 15, part 18 or part 95 of this chapter, the operator of the kit must operate under the authority of an FCC license holder or must secure an [experimental authorization under part 5 of this chapter](https://www.govinfo.gov/content/pkg/CFR-2013-title47-vol1/pdf/CFR-2013-title47-vol1-sec2-803.pdf).

---

**© 2025 Qorvo, Inc. All rights reserved.**
