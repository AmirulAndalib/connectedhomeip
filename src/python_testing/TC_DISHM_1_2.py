#
#    Copyright (c) 2023 Project CHIP Authors
#    All rights reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

import logging

import chip.clusters as Clusters
from chip.clusters.Types import NullValue
from matter_testing_support import MatterBaseTest, async_test_body, default_matter_test_main
from mobly import asserts


class TC_DISHM_1_2(MatterBaseTest):

    async def read_mode_attribute_expect_success(self, endpoint, attribute):
        cluster = Clusters.Objects.DishwasherMode
        return await self.read_single_attribute_check_success(endpoint=endpoint, cluster=cluster, attribute=attribute)

    @async_test_body
    async def test_TC_DISHM_1_2(self):

        self.endpoint = self.user_params.get("endpoint", 1)
        logging.info("This test expects to find this cluster on endpoint 1")

        attributes = Clusters.DishwasherMode.Attributes

        self.print_step(1, "Commissioning, already done")

        if self.check_pics("DISHM.S.A0000"):
            self.print_step(2, "Read SupportedModes attribute")
            supported_modes = await self.read_mode_attribute_expect_success(endpoint=self.endpoint, attribute=attributes.SupportedModes)

            logging.info("SupportedModes: %s" % (supported_modes))

            asserts.assert_greater_equal(len(supported_modes), 2, "SupportedModes must have at least two entries!")
            asserts.assert_less_equal(len(supported_modes), 255, "SupportedModes must have at most 255 entries!")

            supported_modes_dut = []
            for m in supported_modes:
                if m.mode in supported_modes_dut:
                    asserts.fail("SupportedModes must have unique mode values!")
                else:
                    supported_modes_dut.append(m.mode)

            labels = []
            for m in supported_modes:
                if m.label in labels:
                    asserts.fail("SupportedModes must have unique mode label values!")
                else:
                    labels.append(m.label)

            # common mode tags
            commonTags = {0x0: 'Auto',
                          0x1: 'Quick',
                          0x2: 'Quiet',
                          0x3: 'LowNoise',
                          0x4: 'LowEnergy',
                          0x5: 'Vacation',
                          0x6: 'Min',
                          0x7: 'Max',
                          0x8: 'Night',
                          0x9: 'Day'}

            # kUnknownEnumValue may not be defined
            try:
                modeTags = [tag.value for tag in Clusters.DishwasherMode.Enums.ModeTag
                           if tag is not Clusters.DishwasherMode.Enums.ModeTag.kUnknownEnumValue]
            except:
                modeTags = Clusters.DishwasherMode.Enums.ModeTag

            for m in supported_modes:
                for t in m.modeTags:
                    is_mfg = (0x8000 <= t.value and t.value <= 0xBFFF)
                    asserts.assert_true(t.value <= 0xFFFF, "Tag value is > 16 bits")
                    asserts.assert_true(t.value in commonTags.keys() or t.value in modeTags or is_mfg,
                                        "Found a SupportedModes entry with invalid mode tag value!")

                    asserts.assert_true(type(m.label) is str and len(m.label) in range(1, 65),
                                        "TagName is not the appropriate length or type")
                    if t.value == Clusters.DishwasherMode.Enums.ModeTag.kNormal:
                        normal_present = True
            asserts.assert_true(normal_present, "The Supported Modes does not have an entry of Normal(0x4000)")

        if self.check_pics("DISHM.S.A0001"):
            self.print_step(3, "Read CurrentMode attribute")
            current_mode = await self.read_mode_attribute_expect_success(endpoint=self.endpoint, attribute=attributes.CurrentMode)

            logging.info("CurrentMode: %s" % (current_mode))
            asserts.assert_true(current_mode in supported_modes_dut, "CurrentMode is not a supported mode!")

        if self.check_pics("DISHM.S.A0003"):
            self.print_step(4, "Read OnMode attribute")
            on_mode = await self.read_mode_attribute_expect_success(endpoint=self.endpoint, attribute=attributes.OnMode)

            logging.info("OnMode: %s" % (on_mode))
            asserts.assert_true(on_mode in supported_modes_dut or on_mode == NullValue, "OnMode is not a supported mode!")

        if self.check_pics("DISHM.S.A0002"):
            self.print_step(5, "Read StartUpMode attribute")
            startup_mode = await self.read_mode_attribute_expect_success(endpoint=self.endpoint, attribute=attributes.StartUpMode)

            logging.info("StartUpMode: %s" % (startup_mode))
            asserts.assert_true(startup_mode in supported_modes_dut or startup_mode ==
                                NullValue, "StartUpMode is not a supported mode!")


if __name__ == "__main__":
    default_matter_test_main()
