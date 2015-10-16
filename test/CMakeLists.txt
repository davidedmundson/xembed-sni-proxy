set(xembedsniproxy_test_sources
    test.cpp
    fakewatcher.cpp
    ../snidbus.cpp
)

qt5_add_dbus_adaptor(xembedsniproxy_test_sources ../org.kde.StatusNotifierWatcher.xml
                     fakewatcher.h FakeWatcher)

set(statusnotifieritem_xml ../org.kde.StatusNotifierItem.xml)

set_source_files_properties(${statusnotifieritem_xml} PROPERTIES
   NO_NAMESPACE false
   INCLUDE "../snidbus.h"
   CLASSNAME OrgKdeStatusNotifierItem
)
qt5_add_dbus_interface(xembedsniproxy_test_sources ${statusnotifieritem_xml} statusnotifieritem_interface)

add_executable(xembedsniproxy_test ${xembedsniproxy_test_sources})

target_link_libraries(xembedsniproxy_test
    Qt5::Core
    Qt5::Widgets
    Qt5::DBus
    Qt5::Test
)


