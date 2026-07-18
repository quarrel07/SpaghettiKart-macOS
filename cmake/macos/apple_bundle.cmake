# macOS .app bundle assembly for SpaghettiKart.
#
# Builds SpaghettiKart.app directly from the normal build (MACOSX_BUNDLE), generates the
# app icon, bundles the runtime resources the game and the first-run asset extractor
# need into Contents/Resources, relinks non-system dylibs into Contents/Frameworks,
# and ad-hoc codesigns the result so it launches without "damaged app" warnings.
#
# Resource layout the runtime expects on macOS:
#   * Ship::Context::GetAppBundlePath()  -> <App>.app/Contents/Resources  (read-only)
#       - spaghetti.o2r            : packed port assets (GenerateO2R target)
#       - config.yml, yamls/, meta/: asset definitions + mods.toml read by the
#                                    first-run ROM extractor (GameExtractor)
#   * Ship::Context::GetAppDirectoryPath() -> SHIP_HOME (~/Library/Application Support/com.spaghettikart)
#       - mk64.o2r is extracted here on first run from the user's ROM, alongside config/saves/mods

set(MACOS_DIR ${CMAKE_SOURCE_DIR}/cmake/macos)
set(ENTITLEMENTS_FILE ${MACOS_DIR}/entitlements.plist)

option(SPAGHETTI_BUNDLE_DEPS "Relink and bundle dylibs into the .app so it is portable" ON)

# ---------------------------------------------------------------------------
# Bundle metadata. OUTPUT_NAME "SpaghettiKart" makes the bundle SpaghettiKart.app
# with Contents/MacOS/SpaghettiKart, matching CFBundleExecutable in Info.plist
# (the build target itself stays named "Spaghettify").
# ---------------------------------------------------------------------------
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "SpaghettiKart"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${ENTITLEMENTS_FILE}
)

# ---------------------------------------------------------------------------
# App icon: SpaghettiKart.icns generated from icon.png (matches CFBundleIconFile).
# macOS rounds the square automatically, so it reads as a native icon.
# ---------------------------------------------------------------------------
set(ICON_SRC ${CMAKE_SOURCE_DIR}/icon.png)
set(ICONSET_DIR ${CMAKE_BINARY_DIR}/macosx/SpaghettiKart.iconset)
set(ICNS_FILE ${CMAKE_BINARY_DIR}/macosx/SpaghettiKart.icns)
add_custom_command(
    OUTPUT ${ICNS_FILE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ICONSET_DIR}
    COMMAND sips -z 16 16     ${ICON_SRC} --out ${ICONSET_DIR}/icon_16x16.png
    COMMAND sips -z 32 32     ${ICON_SRC} --out ${ICONSET_DIR}/icon_16x16@2x.png
    COMMAND sips -z 32 32     ${ICON_SRC} --out ${ICONSET_DIR}/icon_32x32.png
    COMMAND sips -z 64 64     ${ICON_SRC} --out ${ICONSET_DIR}/icon_32x32@2x.png
    COMMAND sips -z 128 128   ${ICON_SRC} --out ${ICONSET_DIR}/icon_128x128.png
    COMMAND sips -z 256 256   ${ICON_SRC} --out ${ICONSET_DIR}/icon_128x128@2x.png
    COMMAND sips -z 256 256   ${ICON_SRC} --out ${ICONSET_DIR}/icon_256x256.png
    COMMAND sips -z 512 512   ${ICON_SRC} --out ${ICONSET_DIR}/icon_256x256@2x.png
    COMMAND sips -z 512 512   ${ICON_SRC} --out ${ICONSET_DIR}/icon_512x512.png
    COMMAND sips -z 1024 1024 ${ICON_SRC} --out ${ICONSET_DIR}/icon_512x512@2x.png
    COMMAND iconutil -c icns -o ${ICNS_FILE} ${ICONSET_DIR}
    DEPENDS ${ICON_SRC}
    COMMENT "Generating SpaghettiKart.icns from ${ICON_SRC}"
    VERBATIM
)
add_custom_target(SpaghettiKartIcon DEPENDS ${ICNS_FILE})
add_dependencies(${PROJECT_NAME} SpaghettiKartIcon)
set_source_files_properties(${ICNS_FILE} PROPERTIES GENERATED TRUE MACOSX_PACKAGE_LOCATION "Resources")
target_sources(${PROJECT_NAME} PRIVATE ${ICNS_FILE})

# Ensure the packed port assets (spaghetti.o2r) are generated before the app links,
# so the POST_BUILD step below always has them to copy into the bundle. (mk64.o2r is
# created at runtime from the user's ROM into SHIP_HOME, so it is intentionally not a
# build dependency.)
add_dependencies(${PROJECT_NAME} GenerateO2R)

# ---------------------------------------------------------------------------
# Copy runtime resources into Contents/Resources after the app links
# ---------------------------------------------------------------------------
set(RES_DIR "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/config.yml" "${RES_DIR}/config.yml"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/yamls" "${RES_DIR}/yamls"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/meta" "${RES_DIR}/meta"
    # spaghetti.o2r is produced by the GenerateO2R target; copy if present.
    COMMAND bash -c "[ -f '${CMAKE_BINARY_DIR}/spaghetti.o2r' ] && cp '${CMAKE_BINARY_DIR}/spaghetti.o2r' '${RES_DIR}/spaghetti.o2r' || ([ -f '${CMAKE_SOURCE_DIR}/spaghetti.o2r' ] && cp '${CMAKE_SOURCE_DIR}/spaghetti.o2r' '${RES_DIR}/spaghetti.o2r' || echo 'note: spaghetti.o2r not found - build the GenerateO2R target, then rebuild')"
    COMMENT "Bundling SpaghettiKart resources into the .app"
    VERBATIM
)

# ---------------------------------------------------------------------------
# Relink dylibs into Contents/Frameworks (portable .app) and codesign
# ---------------------------------------------------------------------------
if (SPAGHETTI_BUNDLE_DEPS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DAPP_BUNDLE=$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>
            -DEXECUTABLE_NAME=SpaghettiKart
            -P ${MACOS_DIR}/fixup_bundle.cmake
        COMMAND bash -c "install_name_tool -add_rpath '@executable_path/../Frameworks/' '$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/MacOS/SpaghettiKart' 2>/dev/null || true"
        # Homebrew's "sdl2" is sdl2-compat, a shim that dlopen()s libSDL3.dylib from
        # @loader_path at runtime. fixup_bundle can't follow a dlopen, so copy SDL3 in
        # next to the bundled libSDL2 (= @loader_path) by hand or the app aborts with
        # "Failed loading SDL3 library." SDL3 itself only links system frameworks.
        COMMAND bash -c "SDL3_LIB=$(brew --prefix sdl3 2>/dev/null)/lib/libSDL3.0.dylib; [ -f \"$SDL3_LIB\" ] && cp \"$SDL3_LIB\" '$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/libSDL3.dylib' && chmod u+w '$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/libSDL3.dylib' || echo 'warning: libSDL3.dylib not found - install sdl3 via Homebrew'"
        COMMENT "Relinking dylibs into the .app bundle (incl. SDL3 for sdl2-compat)"
        VERBATIM
    )
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND codesign --force --deep --sign - --options runtime --entitlements ${ENTITLEMENTS_FILE} "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
    COMMENT "Ad-hoc codesigning SpaghettiKart.app"
    VERBATIM
)
