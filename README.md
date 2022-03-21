# JPEGView - Image Viewer and Editor

This is the semi-official re-release of JPEGView.

## Description

JPEGView is a lean, fast and highly configurable viewer/editor for JPEG, BMP, PNG, WEBP, TGA, GIF and TIFF images with a minimal GUI. Basic on-the-fly image processing is provided - allowing adjusting typical parameters as sharpness, color balance, rotation, perspective, contrast and local under-/overexposure.

Features
* Small and fast, uses SSE2 and up to 4 CPU cores
* High quality resampling filter, preserving sharpness of images
* Basic image processing tools can be applied realtime during viewing
* Movie mode to play folder of JPEGs as movie

(summary from the original SourceForge project page)

# Installation

## Official Releases

Official releases will be made to [sylikc's GitHub Releases](https://github.com/sylikc/jpegview/releases) page.  It includes:
* **Archive Zip/7z** - Portable
* **Windows Installer MSI** - For Installs
* **Source code** - Build yourself

## Portable

JPEGView _does not require installation_ to run.  Just **unzip, and run** either the 64-bit version, or the 32-bit version depending on which platform you're on.  It can save the settings to the extracted folder and run entirely portable.

## MSI Installer

For those who prefer to have JPEGView installed for All Users, a 32-bit/64-bit installer will be available to download starting with v1.0.40.  I don't own a code signing certificate yet, so the MSI release is not signed.

### WinGet

If you're on Windows 11, or Windows 10 1709 or later, you can also download it directly from the official [Microsoft WinGet tool](https://docs.microsoft.com/en-us/windows/package-manager/winget/) repository.  This downloads the latest MSI installer directly from GitHub for installation.

Example Usage:

C:\> `winget search jpegview`
```
Name     Id              Version  Source
-----------------------------------------
JPEGView sylikc.JPEGView 1.0.39.1 winget
```

C:\> `winget install jpegview`
```
Found JPEGView [sylikc.JPEGView] Version 1.0.39.1
This application is licensed to you by its owner.
Microsoft is not responsible for, nor does it grant any licenses to, third-party packages.
Downloading https://github.com/sylikc/jpegview/releases/download/v1.0.39.1-wix/JPEGView64_en-us_1.0.39.1.msi
  ==============================  2.13 MB / 2.13 MB
Successfully verified installer hash
Starting package install...
Successfully installed
```

## PortableApps

Another option is to use the official [JPEGView on PortableApps](https://portableapps.com/apps/graphics_pictures/jpegview_portable) package.  The PortableApps launcher preserves user settings in a separate directory from the extracted application directory.  This release is signed.

## System Requirements

Windows XP SP2 or later is needed to run the 32 bit version.

64 bit Windows 7 or later is needed to run the 64 bit version.

## What's New

* See what has changed in the [latest releases](https://github.com/sylikc/jpegview/releases)
* Or Check the [CHANGELOG.txt](https://github.com/sylikc/jpegview/blob/master/CHANGELOG.txt) to review new features in detail.


# Brief History

This GitHub repo continues the legacy (is a "fork") of the excellent project [JPEGView by David Kleiner](https://sourceforge.net/projects/jpegview/).  Unfortunately as of 2020, the SourceForge project has essentially been abandoned, with the last update being [2018-02-24 (1.0.37)](https://sourceforge.net/projects/jpegview/files/jpegview/).  It's an excellent lightweight image viewer that I use almost daily!

The starting point for this repo was a direct clone from SourceForge SVN to GitHub Git.  By continuing this way, it retains all previous commits and all original author comments.  

I'm hoping with this project, some devs might help me keep the project alive!  It's been awhile, and could use some new features or updates.  Looking forward to the community making suggestions, and devs will help with some do pull requests as some of the image code is quite a learning curve for me to pick it up. -sylikc
