// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ngraph/graph_util.hpp"
#include "ngraph/ngraph.hpp"
#include "ngraph/opsets/opset.hpp"
#include "ngraph/variant.hpp"

NGRAPH_SUPPRESS_DEPRECATED_START

using namespace std;
using namespace ngraph;

TEST(op, is_op) {
    auto arg0 = make_shared<op::Parameter>(element::f32, Shape{1});
    ASSERT_NE(nullptr, arg0);
    EXPECT_TRUE(op::is_parameter(arg0));
}

TEST(op, is_parameter) {
    auto arg0 = make_shared<op::Parameter>(element::f32, Shape{1});
    ASSERT_NE(nullptr, arg0);
    auto t0 = make_shared<op::v1::Add>(arg0, arg0);
    ASSERT_NE(nullptr, t0);
    EXPECT_FALSE(op::is_parameter(t0));
}

TEST(op, opset_multi_thread) {
    auto doTest = [&](std::function<const ngraph::OpSet&()> fun) {
        std::atomic<const ngraph::OpSet*> opset{nullptr};
        std::atomic_bool failed{false};
        auto threadFun = [&]() {
            const ngraph::OpSet* op = &fun();
            const ngraph::OpSet* current = opset;
            do {
                if (current != nullptr && current != op) {
                    failed = true;
                    break;
                }
            } while (opset.compare_exchange_strong(op, current));
        };
        std::thread t1{threadFun};
        std::thread t2{threadFun};
        t1.join();
        t2.join();
        ASSERT_FALSE(failed);
    };
    doTest(ngraph::get_opset1);
    doTest(ngraph::get_opset2);
    doTest(ngraph::get_opset3);
    doTest(ngraph::get_opset4);
    doTest(ngraph::get_opset5);
    doTest(ngraph::get_opset6);
    doTest(ngraph::get_opset7);
}

struct Ship {
    std::string name;
    int16_t x;
    int16_t y;
};

namespace ov {
template <>
class VariantWrapper<Ship> : public VariantImpl<Ship> {
public:
    OPENVINO_RTTI("VariantWrapper<Ship>");
    VariantWrapper(const value_type& value) : VariantImpl<value_type>(value) {}
};

}  // namespace ov

TEST(op, variant) {
    shared_ptr<Variant> var_std_string = make_variant<std::string>("My string");
    ASSERT_TRUE((ov::is_type<VariantWrapper<std::string>>(var_std_string)));
    EXPECT_EQ((ov::as_type_ptr<VariantWrapper<std::string>>(var_std_string)->get()), "My string");

    shared_ptr<Variant> var_int64_t = make_variant<int64_t>(27);
    ASSERT_TRUE((ov::is_type<VariantWrapper<int64_t>>(var_int64_t)));
    EXPECT_FALSE((ov::is_type<VariantWrapper<std::string>>(var_int64_t)));
    EXPECT_EQ((ov::as_type_ptr<VariantWrapper<int64_t>>(var_int64_t)->get()), 27);

    shared_ptr<Variant> var_ship = make_variant<Ship>(Ship{"Lollipop", 3, 4});
    ASSERT_TRUE((ov::is_type<VariantWrapper<Ship>>(var_ship)));
    Ship& ship = ov::as_type_ptr<VariantWrapper<Ship>>(var_ship)->get();
    EXPECT_EQ(ship.name, "Lollipop");
    EXPECT_EQ(ship.x, 3);
    EXPECT_EQ(ship.y, 4);

    auto node = make_shared<op::Parameter>(element::f32, Shape{1});
    // Check Node RTInfo
    node->get_rt_info()["A"] = var_ship;
    auto node_var_ship = node->get_rt_info().at("A");
    ASSERT_TRUE((ov::is_type<VariantWrapper<Ship>>(node_var_ship)));
    Ship& node_ship = ov::as_type_ptr<VariantWrapper<Ship>>(node_var_ship)->get();
    EXPECT_EQ(&node_ship, &ship);

    // Check Node Input<Node> RTInfo
    auto relu = make_shared<op::Relu>(node);
    relu->input(0).get_rt_info()["A"] = var_ship;
    auto node_input_var_ship = node->get_rt_info().at("A");
    ASSERT_TRUE((ov::is_type<VariantWrapper<Ship>>(node_input_var_ship)));
    Ship& node_input_ship = ov::as_type_ptr<VariantWrapper<Ship>>(node_input_var_ship)->get();
    EXPECT_EQ(&node_input_ship, &ship);

    // Check Node Input<Node> RTInfo
    node->output(0).get_rt_info()["A"] = var_ship;
    auto node_output_var_ship = node->get_rt_info().at("A");
    ASSERT_TRUE((ov::is_type<VariantWrapper<Ship>>(node_output_var_ship)));
    Ship& node_output_ship = ov::as_type_ptr<VariantWrapper<Ship>>(node_input_var_ship)->get();
    EXPECT_EQ(&node_output_ship, &ship);
}
