# Make sure it produce shami.so instead of libshami.so
find_package(PkgConfig REQUIRED)
pkg_check_modules(CHEWING REQUIRED chewing)
include_directories(${CHEWING_INCLUDE_DIRS})
link_directories(${CHEIWNG_LIBRARY_DIRS})
add_definitions(${CHEWING_CFLAGS_OTHER})

# add_library(boshiamy_data STATIC boshiamy_data.hpp boshiamy_data.cpp)
add_library(boshiamy_data SHARED boshiamy_data.hpp boshiamy_data.cpp)
add_library(shami SHARED shami.cpp)
target_link_libraries(shami 
	PRIVATE Fcitx5::Core boshiamy_data ${CHEWING_LIBRARIES})
set_target_properties(shami PROPERTIES PREFIX "")
install(TARGETS shami DESTINATION "${FCITX_INSTALL_LIBDIR}/fcitx5")

# Addon config file
# We need additional layer of conversion because we want PROJECT_VERSION in it.
configure_file(shami-addon.conf.in shami-addon.conf)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/shami-addon.conf" RENAME shami.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

# Input Method registration file
install(FILES "shami.conf" DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")
