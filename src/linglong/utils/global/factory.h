/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_CLI_APP_MANAGER_FACTORY_H
#define LINGLONG_SRC_CLI_APP_MANAGER_FACTORY_H

#include "linglong/api/v1/dbus/app_manager1.h"
#include "linglong/api/v1/dbus/package_manager1.h"
#include "linglong/package_manager/package_manager.h"

#include <QDBusConnection>
#include <QString>

#include <memory>
#include <mutex>

// move utils
template<typename T>
class Factory
{
public:
    Factory(std::function<std::shared_ptr<T>()> fn)
        : fn(fn)
    {
    }

    std::shared_ptr<T> builder()
    {
        std::lock_guard _(mu);
        if (this->value == nullptr) {
            return this->value = this->fn();
        } else {
            return this->value;
        }
    }

    ~Factory() = default;

private:
    std::mutex mu;
    std::function<std::shared_ptr<T>()> fn;
    std::shared_ptr<T> value;
};

extern template class Factory<linglong::api::v1::dbus::AppManager1>;
extern template class Factory<OrgDeepinLinglongPackageManager1Interface>;
extern template class Factory<linglong::service::PackageManager>;

// ---
// 从外部注入对象构造指针，让需要调用此方法的类调用构造函数进行对象构造。
// 但是为了配合单元测试，可能需要构造不同的对象，如果对象之间有继承关系，则可以使用
// 工厂模板类，来配合类之间的重写，实现调用不同方法。
// template class Factory<linglong::api::v1::dbus::AppManager1>;

// CLI::xxx()
// {
//     auto appMan = this->f();
//     appMan->xxx;
// }

// int fn()
// {
//     Factory<linglong::api::v1::dbus::AppManager1> f([]() {
//         return std::make_shared<linglong::api::v1::dbus::AppManager1>();
//     });
//     Cli cli(&f);
//     int ret = cli.xxxx();
//     ASSERT_EQ(ret, 0);
// }

#endif // LINGLONG_SRC_CLI_APP_MANAGER_FACTORY_H