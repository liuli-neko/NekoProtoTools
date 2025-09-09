#include <gtest/gtest.h>

#include "nekoproto/serialization/serializer_base.hpp"

#ifdef NEKO_PROTO_ENABLE_RAPIDXML
#include <rapidxml/rapidxml.hpp>

#include "nekoproto/serialization/types/enum.hpp"
#include "nekoproto/serialization/types/vector.hpp"
#include "nekoproto/serialization/types/struct_unwrap.hpp"
#include "nekoproto/serialization/xml_serializer.hpp"

const char* gXmlData = R"(<breakfast_menu>
    <food>
        <name>Belgian Waffles</name>
        <price>$5.95</price>
        <description>Two of our famous Belgian Waffles with plenty of real maple syrup</description>
        <calories>650</calories>
    </food>
    <food>
        <name>Strawberry Belgian Waffles</name>
        <price>$7.95</price>
        <description>Light Belgian waffles covered with strawberries and whipped cream</description>
        <calories>900</calories>
    </food>
    <food>
        <name>Berry-Berry Belgian Waffles</name>
        <price>$8.95</price>
        <description>Light Belgian waffles covered with an assortment of fresh berries and whipped cream</description>
        <calories>900</calories>
    </food>
    <food>
        <name>French Toast</name>
        <price>$4.50</price>
        <description>Thick slices made from our homemade sourdough bread</description>
        <calories>600</calories>
    </food>
    <food>
        <name>Homestyle Breakfast</name>
        <price>$6.95</price>
        <description>Two eggs, bacon or sausage, toast, and our ever-popular hash browns</description>
        <calories>950</calories>
    </food>
</breakfast_menu>)";

struct Food {
    std::string name;
    std::string price;
    std::string description;
    int calories;

    NEKO_SERIALIZER(name, price, description, calories)
};

struct BreakfastMenu {
    std::vector<Food> breakfast_menu; // NOLINT(readability-identifier-naming)

    NEKO_SERIALIZER(breakfast_menu)
};

TEST(XML, Parse) {
    rapidxml::xml_document<> doc;
    std::string xml = gXmlData;
    BreakfastMenu breakfastMenu;
    {
        NEKO_NAMESPACE::RapidXmlInputSerializer<> serializer(xml.data(), xml.size());
        serializer(breakfastMenu);
    }
    ASSERT_EQ(breakfastMenu.breakfast_menu.size(), 5);
    EXPECT_STREQ(breakfastMenu.breakfast_menu[0].name.c_str(), "Belgian Waffles");
    EXPECT_STREQ(breakfastMenu.breakfast_menu[0].price.c_str(), "$5.95");
    EXPECT_STREQ(breakfastMenu.breakfast_menu[0].description.c_str(),
                 "Two of our famous Belgian Waffles with plenty of real maple syrup");
    EXPECT_EQ(breakfastMenu.breakfast_menu[0].calories, 650);
    EXPECT_STREQ(breakfastMenu.breakfast_menu[1].name.c_str(), "Strawberry Belgian Waffles");
    EXPECT_STREQ(breakfastMenu.breakfast_menu[1].price.c_str(), "$7.95");
    EXPECT_STREQ(breakfastMenu.breakfast_menu[1].description.c_str(),
                 "Light Belgian waffles covered with strawberries and whipped cream");
    EXPECT_EQ(breakfastMenu.breakfast_menu[1].calories, 900);
    EXPECT_STREQ(breakfastMenu.breakfast_menu[2].name.c_str(), "Berry-Berry Belgian Waffles");
    EXPECT_STREQ(breakfastMenu.breakfast_menu[2].price.c_str(), "$8.95");
    EXPECT_STREQ(breakfastMenu.breakfast_menu[2].description.c_str(),
                 "Light Belgian waffles covered with an assortment of fresh berries and whipped cream");
    EXPECT_EQ(breakfastMenu.breakfast_menu[2].calories, 900);
    EXPECT_STREQ(breakfastMenu.breakfast_menu[3].name.c_str(), "French Toast");
    EXPECT_STREQ(breakfastMenu.breakfast_menu[3].price.c_str(), "$4.50");
    EXPECT_STREQ(breakfastMenu.breakfast_menu[3].description.c_str(),
                 "Thick slices made from our homemade sourdough bread");
    EXPECT_EQ(breakfastMenu.breakfast_menu[3].calories, 600);
    EXPECT_STREQ(breakfastMenu.breakfast_menu[4].name.c_str(), "Homestyle Breakfast");
    EXPECT_STREQ(breakfastMenu.breakfast_menu[4].price.c_str(), "$6.95");
    EXPECT_STREQ(breakfastMenu.breakfast_menu[4].description.c_str(),
                 "Two eggs, bacon or sausage, toast, and our ever-popular hash browns");
    EXPECT_EQ(breakfastMenu.breakfast_menu[4].calories, 950);
}
#endif
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_INFO);
    NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_DEBUG);
    return RUN_ALL_TESTS();
}