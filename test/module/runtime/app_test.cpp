/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Heysion <heysion@deepin.com>
 *
 * Maintainer: Heysion <heysion@deepin.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QFile>
#include <QDebug>

#include "src/module/util/yaml.h"
#include "src/module/runtime/oci.h"
#include "src/module/runtime/app.h"
#include "src/module/runtime/runtime.h"

TEST(AppTest, StructTest)
{
    linglong::runtime::registerAllMetaType();

    // load data from yaml file
    auto yamlLoad = YAML::LoadFile("../../test/data/demo/app-test.yaml");
    EXPECT_NE(yamlLoad.size(), 0);

    auto appYaml = formYaml<linglong::runtime::App>(yamlLoad);
    EXPECT_NE(appYaml, nullptr);

    // check base info
    EXPECT_EQ(appYaml->version, "1.0");
    EXPECT_EQ(appYaml->package->ref, "org.deepin.calculator/5.7.16/x86_64");
    EXPECT_EQ(appYaml->runtime->ref, "org.deepin.Runtime/20.5.0/x86_64");

    // check permission
    EXPECT_NE(appYaml->permissions, nullptr);
    EXPECT_NE(appYaml->permissions->mounts.size(), 0);
    EXPECT_EQ(appYaml->permissions->mounts[0]->type, "bind");
    EXPECT_EQ(appYaml->permissions->mounts[0]->options, "rw,rbind");
    EXPECT_EQ(appYaml->permissions->mounts[0]->source, "/home/linglong/Desktop");
    EXPECT_EQ(appYaml->permissions->mounts[0]->destination, "/home/linglong/Desktop");

    appYaml->deleteLater();
}
