set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_PROVIDED_FORTRAN ON)

# Overrides the built-in triplet of the same name, so keep the lines above identical to
# it. WebView2 is the one port we take statically: its loader DLL is Microsoft's to sign,
# and WhttBrowserLogin.dll links the static one instead.
if(PORT STREQUAL "webview2")
    set(VCPKG_LIBRARY_LINKAGE static)
endif()
