/******************************************************************************
 *
 * Copyright (c) 2012 - 2024 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/
#include "dev.h"
#include "reg_info.h"
#include "debug.h"
#include "utils.h"
#include <linux/fs.h>

#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

#define SLSI_REG_INFO_BUILD_REG_DOM_VER(major, minor, minor2) \
		((major & 0xFF) << 16) | ((minor & 0xFF) << 8) | (minor2 & 0xFF)


void slsi_regd_use_world_domain(struct slsi_dev *sdev)
{
	struct ieee80211_regdomain *slsi_world_regdom_custom = sdev->device_config.domain_info.regdomain;
	struct ieee80211_reg_rule  reg_rules[] = {
		/* Channel 1 - 11 */
		REG_RULE(2412 - 10, 2462 + 10, 40, 0, 20, 0),
		/* Channel 12 - 13 NO_IR */
		REG_RULE(2467 - 10, 2472 + 10, 20, 0, 20, NL80211_RRF_NO_IR),
		/* Channel 14 NO_IR and NO_OFDM */
		REG_RULE(2484 - 10, 2484 + 10, 20, 0, 20, NL80211_RRF_NO_IR | NL80211_RRF_NO_OFDM),
		/* Channel 36 - 48 */
		REG_RULE(5180 - 10, 5240 + 10, 80, 0, 20, 0),
		/* Channel 52 - 64 */
		REG_RULE(5260 - 10, 5320 + 10, 80, 0, 20, NL80211_RRF_DFS),
		/* Channel 100 - 144 */
		REG_RULE(5500 - 10, 5720 + 10, 80, 0, 20, NL80211_RRF_DFS),
		/* Channel 149 - 165 */
		REG_RULE(5745 - 10, 5825 + 10, 80, 0, 20, 0),
	};

	int                        i;

	SLSI_DBG1_NODEV(SLSI_INIT_DEINIT, "regulatory init\n");
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	slsi_world_regdom_custom->n_reg_rules = (sizeof(reg_rules)) / sizeof(reg_rules[0]);
	for (i = 0; i < slsi_world_regdom_custom->n_reg_rules; i++)
		slsi_world_regdom_custom->reg_rules[i] = reg_rules[i];

	/* Country code '00' indicates world regulatory domain */
	slsi_world_regdom_custom->alpha2[0] = '0';
	slsi_world_regdom_custom->alpha2[1] = '0';
}

void slsi_regd_init(struct slsi_dev *sdev)
{
	slsi_regd_use_world_domain(sdev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0))
	regulatory_set_wiphy_regd(sdev->wiphy, sdev->device_config.domain_info.regdomain);
#else
	wiphy_apply_custom_regulatory(sdev->wiphy, sdev->device_config.domain_info.regdomain);
#endif
}

void slsi_regd_init_wiphy_not_registered(struct slsi_dev *sdev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0))
	enum nl80211_band band;
	struct wiphy *wiphy = NULL;
	struct ieee80211_supported_band *sband = NULL;
	struct ieee80211_regdomain *slsi_world_regdom_custom = NULL;
	struct ieee80211_regdomain *new_regd = NULL;
	struct ieee80211_channel *chan = NULL;
	int i = 0;
	int j = 0;
	int index = -1;
	u32 bw_flags = 0;
	u32 max_bandwidth_khz = 0;
	u32 center_freq_khz = 0;
	u32 start_freq_khz = 0;
	u32 end_freq_khz = 0;
	u32 flags = 0;
	u32 channel_flags = 0;

	slsi_regd_use_world_domain(sdev);
	slsi_world_regdom_custom = sdev->device_config.domain_info.regdomain;
	wiphy = sdev->wiphy;
	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		sband = wiphy->bands[band];
		if (!sband)
			continue;
		for (i = 0; i < sband->n_channels; i++) {
			chan = &sband->channels[i];
			if (!chan)
				continue;
			center_freq_khz = ieee80211_channel_to_khz(chan);
			bw_flags = 0;
			channel_flags = 0;
			index = -1;
			for (j = 0; j < slsi_world_regdom_custom->n_reg_rules; j++) {
				start_freq_khz = slsi_world_regdom_custom->reg_rules[j].freq_range.start_freq_khz;
				end_freq_khz = slsi_world_regdom_custom->reg_rules[j].freq_range.end_freq_khz;
				if (((center_freq_khz - MHZ_TO_KHZ(10)) >= start_freq_khz)
				      && ((center_freq_khz + MHZ_TO_KHZ(10)) <= end_freq_khz)) {
					index = j;
					break;
				}
			}
			if (index != -1) {
				max_bandwidth_khz = slsi_world_regdom_custom->reg_rules[index].freq_range.max_bandwidth_khz;
				if (max_bandwidth_khz < MHZ_TO_KHZ(40))
					bw_flags |= IEEE80211_CHAN_NO_HT40;
				if (max_bandwidth_khz < MHZ_TO_KHZ(80))
					bw_flags |= IEEE80211_CHAN_NO_80MHZ;
				if (max_bandwidth_khz < MHZ_TO_KHZ(160))
					bw_flags |= IEEE80211_CHAN_NO_160MHZ;

				flags = slsi_world_regdom_custom->reg_rules[index].flags;
				if (flags & NL80211_RRF_NO_IR)
					channel_flags |= IEEE80211_CHAN_NO_IR;
				if (flags & NL80211_RRF_DFS)
					channel_flags |= IEEE80211_CHAN_RADAR;
				if (flags & NL80211_RRF_NO_OUTDOOR)
					channel_flags |= IEEE80211_CHAN_INDOOR_ONLY;
				if (flags & NL80211_RRF_NO_OFDM)
					channel_flags |= IEEE80211_CHAN_NO_OFDM;
			} else {
				channel_flags = IEEE80211_CHAN_DISABLED;
			}
			chan->flags = bw_flags | channel_flags;
		}
	}
	new_regd = kzalloc(struct_size(new_regd, reg_rules,
			   slsi_world_regdom_custom->n_reg_rules), GFP_KERNEL);
	if (!new_regd) {
		SLSI_ERR(sdev, "Memory Allocation Failure for new_regd\n");
		return;
	}
	memcpy(new_regd, slsi_world_regdom_custom, sizeof(struct ieee80211_regdomain));
	for (i = 0; i < slsi_world_regdom_custom->n_reg_rules; i++)
		memcpy(&new_regd->reg_rules[i], &slsi_world_regdom_custom->reg_rules[i],
		       sizeof(struct ieee80211_reg_rule));
	rcu_assign_pointer(wiphy->regd, new_regd);
#else
	slsi_regd_init(sdev);
#endif
}

void slsi_regd_deinit(struct slsi_dev *sdev)
{
	SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "\n");

	kfree(sdev->device_config.domain_info.countrylist);
	sdev->device_config.domain_info.countrylist = NULL;
	kfree(sdev->regdb.freq_ranges);
	sdev->regdb.freq_ranges = NULL;
	kfree(sdev->regdb.reg_rules);
	sdev->regdb.reg_rules = NULL;
	kfree(sdev->regdb.rules_collection);
	sdev->regdb.rules_collection = NULL;
	kfree(sdev->regdb.country);
	sdev->regdb.country = NULL;
}

int slsi_read_regulatory(struct slsi_dev *sdev)
{
#if IS_ENABLED(CONFIG_SCSC_PCIE)
	char *reg_file_t = "slsi_reg_database.bin";
#else
	char *reg_file_t = "wifi/slsi_reg_database.bin";
	char *reg_file_t_legacy = "../etc/wifi/slsi_reg_database.bin";
#endif
	int i = 0, j = 0, index = 0;
	u32 num_freqbands = 0, num_rules = 0, num_collections = 0;
	int script_version = 0;
	int r = 0, offset = 0;
	const struct firmware *firm;

	if (sdev->regdb.regdb_state == SLSI_REG_DB_SET) {
		SLSI_INFO(sdev, "DB Ver:%d.%d.%d, Num Countries:%d\n", sdev->regdb.db_major_version,
			  sdev->regdb.db_minor_version, sdev->regdb.db_2nd_minor_version, sdev->regdb.num_countries);
		sdev->reg_dom_version = SLSI_REG_INFO_BUILD_REG_DOM_VER(sdev->regdb.db_major_version,
									sdev->regdb.db_minor_version,
									sdev->regdb.db_2nd_minor_version);
		return 0;
	}

	r = mx140_request_file(sdev->maxwell_core, reg_file_t, &firm);
	if (r) {
		SLSI_INFO(sdev, "Error Loading %s file %d\n", reg_file_t, r);
		r = mx140_request_file(sdev->maxwell_core, reg_file_t_legacy, &firm);
		if (r) {
			SLSI_INFO(sdev, "Error Loading 2nd %s file %d\n", reg_file_t_legacy, r);
			sdev->regdb.regdb_state = SLSI_REG_DB_ERROR;
			return -EINVAL;
		}
	}

	if (firmware_read(firm, &script_version, sizeof(uint32_t), &offset) < 0) {
		SLSI_INFO(sdev, "Failed to read script version\n");
		goto exit;
	}
	if (script_version > 3) {
		SLSI_INFO(sdev, "DB unknown script version:%d. Abort DB file read\n", script_version);
		goto exit;
	}
	if (firmware_read(firm, &sdev->regdb.db_major_version, sizeof(uint32_t), &offset) < 0) {
		SLSI_INFO(sdev, "Failed to read regdb major version %u\n", sdev->regdb.db_major_version);
		goto exit;
	}
	if (firmware_read(firm, &sdev->regdb.db_minor_version, sizeof(uint32_t), &offset) < 0) {
		SLSI_INFO(sdev, "Failed to read regdb minor version %u\n", sdev->regdb.db_minor_version);
		goto exit;
	}

	/*2nd minor number is introduced from script version 3 onwards.*/
	sdev->regdb.db_2nd_minor_version = 0;
	if (script_version >= 3) {
		if (firmware_read(firm, &sdev->regdb.db_2nd_minor_version, sizeof(uint32_t), &offset) < 0) {
			SLSI_INFO(sdev, "Failed to read regdb 2nd minor version %u\n", sdev->regdb.db_minor_version);
			goto exit;
		}
	}
	if (firmware_read(firm, &num_freqbands, sizeof(uint32_t), &offset) < 0) {
		SLSI_INFO(sdev, "Failed to read Number of Frequency bands %u\n", num_freqbands);
		goto exit;
	}

	sdev->regdb.freq_ranges = kmalloc(sizeof(*sdev->regdb.freq_ranges) * num_freqbands, GFP_KERNEL);
	if (!sdev->regdb.freq_ranges) {
		SLSI_ERR(sdev, "kmalloc of sdev->regdb->freq_ranges failed\n");
		goto exit;
	}
	for (i = 0; i < num_freqbands; i++) {
		if (firmware_read(firm, &sdev->regdb.freq_ranges[i], sizeof(struct regdb_file_freq_range), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb freq_ranges\n");
			goto exit_freq_ranges;
		}
	}

	if (firmware_read(firm, &num_rules, sizeof(uint32_t), &offset) < 0) {
		SLSI_ERR(sdev, "Failed to read num_rules\n");
		goto exit_freq_ranges;
	}

	sdev->regdb.reg_rules = kmalloc(sizeof(*sdev->regdb.reg_rules) * num_rules, GFP_KERNEL);
	if (!sdev->regdb.reg_rules) {
		SLSI_ERR(sdev, "kmalloc of sdev->regdb->reg_rules failed\n");
		goto exit_freq_ranges;
	}
	for (i = 0; i < num_rules; i++) {
		if (firmware_read(firm, &index, sizeof(uint32_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read index\n");
			goto exit_reg_rules;
		}
		sdev->regdb.reg_rules[i].freq_range = &sdev->regdb.freq_ranges[index];
		if (firmware_read(firm, &sdev->regdb.reg_rules[i].max_eirp, sizeof(uint32_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb reg_rules\n");
			goto exit_reg_rules;
		}
		if (firmware_read(firm, &sdev->regdb.reg_rules[i].flags, sizeof(uint32_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb flags\n");
			goto exit_reg_rules;
		}
	}

	if (firmware_read(firm, &num_collections, sizeof(uint32_t), &offset) < 0) {
		SLSI_ERR(sdev, "Failed to read num_collections\n");
		goto exit_reg_rules;
	}

	sdev->regdb.rules_collection = kmalloc(sizeof(*sdev->regdb.rules_collection) * num_collections, GFP_KERNEL);
	if (!sdev->regdb.rules_collection) {
		SLSI_ERR(sdev, "kmalloc of sdev->regdb->rules_collection failed\n");
		goto exit_reg_rules;
	}
	for (i = 0; i < num_collections; i++) {
		if (firmware_read(firm, &sdev->regdb.rules_collection[i].reg_rule_num, sizeof(uint32_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb rules_collection reg_rule_num\n");
			goto exit_rules_collection;
		}
		for (j = 0; j < sdev->regdb.rules_collection[i].reg_rule_num; j++) {
			if (firmware_read(firm, &index, sizeof(uint32_t), &offset) < 0) {
				SLSI_ERR(sdev, "Failed to read regdb rules collections index\n");
				goto exit_rules_collection;
			}
			sdev->regdb.rules_collection[i].reg_rule[j] = &sdev->regdb.reg_rules[index];
		}
	}

	if (firmware_read(firm, &sdev->regdb.num_countries, sizeof(uint32_t), &offset) < 0) {
		SLSI_ERR(sdev, "Failed to read regdb number of countries\n");
		goto exit_rules_collection;
	}
	SLSI_INFO(sdev, "DB Ver:%d.%d.%d, Script Ver:%d, Num Countries:%d\n", sdev->regdb.db_major_version,
		  sdev->regdb.db_minor_version, sdev->regdb.db_2nd_minor_version, script_version,
		  sdev->regdb.num_countries);

	sdev->regdb.country = kmalloc_array(sdev->regdb.num_countries, sizeof(*sdev->regdb.country), GFP_KERNEL);
	if (!sdev->regdb.country) {
		SLSI_ERR(sdev, "kmalloc of sdev->regdb->country failed\n");
		goto exit_rules_collection;
	}
	for (i = 0; i < sdev->regdb.num_countries; i++) {
		if (firmware_read(firm, &sdev->regdb.country[i].alpha2, 2 * sizeof(uint8_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb country alpha2\n");
			goto exit_country;
		}
		if (firmware_read(firm, &sdev->regdb.country[i].pad_byte, sizeof(uint8_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb country pad_byte\n");
			goto exit_country;
		}
		if (firmware_read(firm, &sdev->regdb.country[i].operating_class_set, sizeof(uint8_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb country operating_class_set\n");
			goto exit_country;
		}
		if (firmware_read(firm, &index, sizeof(uint32_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb country index\n");
			goto exit_country;
		}
		sdev->regdb.country[i].collection = &sdev->regdb.rules_collection[index];
	}

	mx140_release_file(sdev->maxwell_core, firm);
	sdev->regdb.regdb_state = SLSI_REG_DB_SET;
	sdev->reg_dom_version = SLSI_REG_INFO_BUILD_REG_DOM_VER(sdev->regdb.db_major_version,
								sdev->regdb.db_minor_version,
								sdev->regdb.db_2nd_minor_version);
	return 0;

exit_country:
	kfree(sdev->regdb.country);
	sdev->regdb.country = NULL;

exit_rules_collection:
	kfree(sdev->regdb.rules_collection);
	sdev->regdb.rules_collection = NULL;

exit_reg_rules:
	kfree(sdev->regdb.reg_rules);
	sdev->regdb.reg_rules = NULL;

exit_freq_ranges:
	kfree(sdev->regdb.freq_ranges);
	sdev->regdb.freq_ranges = NULL;
exit:
	sdev->regdb.regdb_state = SLSI_REG_DB_ERROR;
	mx140_release_file(sdev->maxwell_core, firm);
	return -EINVAL;
}
