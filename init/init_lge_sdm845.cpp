/*
 * Copyright (C) 2019 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#include <sys/sysinfo.h>

#include "property_service.h"
#include "vendor_init.h"

using android::base::Trim;

void property_override(const std::string& name, const std::string& value) {
    size_t valuelen = value.size();

    prop_info* pi = (prop_info*) __system_property_find(name.c_str());
    if (pi != nullptr) {
        __system_property_update(pi, value.c_str(), valuelen);
    }
    else {
        int rc = __system_property_add(name.c_str(), name.size(), value.c_str(), valuelen);
        if (rc < 0) {
            LOG(ERROR) << "property_set(\"" << name << "\", \"" << value << "\") failed: "
                       << "__system_property_add failed";
        }
    }
}

void property_override_multifp(char const buildfp[], char const systemfp[],
	char const bootimagefp[], char const vendorfp[], char const value[])
{
	property_override(buildfp, value);
	property_override(systemfp, value);
	property_override(bootimagefp, value);
	property_override(vendorfp, value);
}

void init_target_properties() {
    std::string model;
    std::string product_name;
    std::string cmdline;
    std::string cust_prop_name = "cust.prop";
    std::string default_cust_prop_path = "/oem/OP/" + cust_prop_name;
    std::string cust_prop_path;
    std::string cust_prop_line;
    DIR *dir;
    struct dirent *ent;
    struct stat statBuf;
    bool unknownModel = true;
    bool dualSim = false;

    android::base::ReadFileToString("/proc/cmdline", &cmdline);

    for (const auto& entry : android::base::Split(android::base::Trim(cmdline), " ")) {
        std::vector<std::string> pieces = android::base::Split(entry, "=");
        if (pieces.size() == 2) {
            if(pieces[0].compare("androidboot.vendor.lge.product.model") == 0)
            {
                model = pieces[1];
                unknownModel = false;
            } else if(pieces[0].compare("androidboot.vendor.lge.sim_num") == 0 && pieces[1].compare("2") == 0)
            {
                dualSim = true;
            }
        }
    }

    cust_prop_path = default_cust_prop_path;

    if((dir = opendir("/oem/OP/")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if(ent->d_type == DT_DIR) {
                std::string tmp = "/oem/OP/";
                tmp.append(ent->d_name);
                tmp.append("/");
                tmp.append(cust_prop_name);
                if(stat(tmp.c_str(), &statBuf) == 0)
                    cust_prop_path = tmp;
            }
        }
        closedir (dir);
    }
    std::ifstream cust_prop_stream(cust_prop_path, std::ifstream::in);

    while(std::getline(cust_prop_stream, cust_prop_line)) {
        std::vector<std::string> pieces = android::base::Split(cust_prop_line, "=");
        if (pieces.size() == 2) {
            if(pieces[0].compare("ro.vendor.lge.build.target_region") == 0 ||
               pieces[0].compare("ro.vendor.lge.build.target_operator") == 0 ||
               pieces[0].compare("ro.vendor.lge.build.target_country") == 0 ||
               pieces[0].compare("telephony.lteOnCdmaDevice") == 0 ||
               pieces[0].compare("persist.vendor.lge.audio.voice.clarity") == 0)
            {
                property_override(pieces[0], pieces[1]);
            }
        }
    }

    if(unknownModel) {
        model = "UNKNOWN";
    }

    if(dualSim) {
        property_override("persist.radio.multisim.config", "dsds");
    }

    property_override("ro.product.model", model);
    property_override("ro.product.odm.model", model);
    property_override("ro.product.product.model", model);
    property_override("ro.product.system.model", model);
    property_override("ro.product.vendor.model", model);
}

void vendor_load_properties() {
    LOG(INFO) << "Loading vendor specific properties";
    init_target_properties();
    LOG(INFO) << "Loading Coral Fingerprint";
	property_override_multifp("ro.build.fingerprint", "ro.system.build.fingerprint", "ro.bootimage.build.fingerprint",
	    "ro.vendor.build.fingerprint", "google/coral/coral:11/RP1A.201105.002/6869500:user/release-keys");

}
