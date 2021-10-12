/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:    huqinghong@uniontech.com
 *
 * Maintainer:huqinghong@uniontech.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "repohelper.h"
#include "../util/httpclient.h"

namespace linglong {

/*
 * 创建本地repo仓库,当目标路径不存在时，自动新建路径
 *
 * @param qrepoPath: 本地仓库路径
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool RepoHelper::ensureRepoEnv(const QString qrepoPath, QString &err)
{
    const string repoPath = qrepoPath.toStdString();
    GCancellable *cancellable = NULL;
    GError *error = NULL;
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (repoPath.empty()) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        // err = QString(QLatin1String(info));
        return false;
    }
    OstreeRepo *repo;
    g_autoptr(GFile) repodir = NULL;
    string tmpPath = "";
    //适配目标路径末尾的‘/’，本地仓库目录名为repo
    if (repoPath.at(repoPath.size() - 1) == '/') {
        tmpPath = repoPath + "repo";
    } else {
        tmpPath = repoPath + "/repo";
    }
    // fprintf(stdout, "ensureRepoEnv repo path:%s\n", tmpPath.c_str());
    qInfo() << "ensureRepoEnv repo path:" << QString::fromStdString(tmpPath);
    repodir = g_file_new_for_path(tmpPath.c_str());
    //判断本地仓库repo目录是否存在
    if (!g_file_query_exists(repodir, cancellable)) {
        // fprintf(stdout, "ensureRepoEnv repo path:%s not exist create dir\n", tmpPath.c_str());
        qInfo() << "ensureRepoEnv repo path:" << QString::fromStdString(tmpPath) << " not exist create dir";
        // 创建目录
        if (g_mkdir_with_parents(tmpPath.c_str(), 0755)) {
            // fprintf(stdout, "g_mkdir_with_parents fialed\n");
            qInfo() << "g_mkdir_with_parents fialed";
            snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s g_mkdir_with_parents fialed", __FILE__, __func__);
            // err = info;
            err = QString(QLatin1String(info));
            return false;
        }
        repo = ostree_repo_new(repodir);
        OstreeRepoMode mode = OSTREE_REPO_MODE_BARE_USER_ONLY;
        // 创建ostree仓库
        if (!ostree_repo_create(repo, mode, cancellable, &error)) {
            // fprintf(stdout, "ostree_repo_create error:%s\n", error->message);
            qInfo() << "ostree_repo_create error:" << error->message;
            snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_create error:%s", __FILE__, __func__, error->message);
            // err = info;
            err = QString(QLatin1String(info));
            return false;
        }
    } else {
        repo = ostree_repo_new(repodir);
    }
    // 校验创建的仓库是否ok
    if (!ostree_repo_open(repo, cancellable, &error)) {
        // fprintf(stdout, "ostree_repo_open:%s error:%s\n", repoPath.c_str(), error->message);
        qInfo() << "ostree_repo_open error:" << error->message;
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_open:%s error:%s", __FILE__, __func__, repoPath.c_str(), error->message);
        // err = info;
        err = QString(QLatin1String(info));
        return false;
    }

    // set dir path info
    this->linglong_dir->setDirInfo(repoPath, repo);

    return true;
}

/*
 * 查询ostree远端仓库列表
 *
 * @param qrepoPath: 远端仓库对应的本地仓库路径
 * @param vec: 远端仓库列表
 * @param err: 错误信息
 *
 * @return bool: true:查询成功 false:失败
 */
bool RepoHelper::getRemoteRepoList(const QString qrepoPath, QVector<QString> &vec, QString &err)
{
    string repoPath = qrepoPath.toStdString();
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (repoPath.empty()) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        // err = info;
        err = QString(QLatin1String(info));
        return false;
    }
    // 校验本地仓库是否创建
    if (!linglong_dir->repo || linglong_dir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        // err = info;
        err = QString(QLatin1String(info));
        return false;
    }
    g_auto(GStrv) res = NULL;
    if (linglong_dir->repo) {
        res = ostree_repo_remote_list(linglong_dir->repo, NULL);
    }

    if (res != NULL) {
        for (int i = 0; res[i] != NULL; i++) {
            // vec.push_back(res[i]);
            vec.append(QLatin1String(res[i]));
        }
    } else {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s no remote repo found", __FILE__, __func__);
        // err = info;
        err = QString(QLatin1String(info));
        return false;
    }

    return true;
}

/*
 * 查询远端ostree仓库描述文件Summary信息
 *
 * @param repo: 远端仓库对应的本地仓库OstreeRepo对象
 * @param name: 远端仓库名称
 * @param outSummary: 远端仓库的Summary信息
 * @param outSummarySig: 远端仓库的Summary签名信息
 * @param cancellable: GCancellable对象
 * @param error: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool RepoHelper::fetchRemoteSummary(OstreeRepo *repo, const char *name, GBytes **outSummary, GBytes **outSummarySig, GCancellable *cancellable, GError **error)
{
    g_autofree char *url = NULL;
    g_autoptr(GBytes) summary = NULL;
    g_autoptr(GBytes) summarySig = NULL;

    if (!ostree_repo_remote_get_url(repo, name, &url, error)) {
        // fprintf(stdout, "fetchRemoteSummary ostree_repo_remote_get_url error:%s\n", (*error)->message);
        qInfo() << "fetchRemoteSummary ostree_repo_remote_get_url error:" << (*error)->message;
        return false;
    }
    fprintf(stdout, "fetchRemoteSummary remote %s,url:%s\n", name, url);

    if (!ostree_repo_remote_fetch_summary(repo, name, &summary, &summarySig, cancellable, error)) {
        // fprintf(stdout, "fetchRemoteSummary ostree_repo_remote_fetch_summary error:%s\n", (*error)->message);
        qInfo() << "fetchRemoteSummary ostree_repo_remote_fetch_summary error:" << (*error)->message;
        return false;
    }

    if (summary == NULL) {
        // fprintf(stdout, "fetch summary error");
        qInfo() << "fetch summary error";
        return false;
    }
    *outSummary = (GBytes *)g_steal_pointer(&summary);
    if (outSummarySig)
        *outSummarySig = (GBytes *)g_steal_pointer(&summarySig);
    return true;
}

/*
 * 从summary中的refMap中获取仓库所有软件包索引refs
 *
 * @param ref_map: summary信息中解析出的ref map信息
 * @param outRefs: 仓库软件包索引信息
 */
void RepoHelper::getPkgRefsFromRefsMap(GVariant *ref_map, map<string, string> &outRefs)
{
    GVariant *value;
    GVariantIter ref_iter;
    g_variant_iter_init(&ref_iter, ref_map);
    while ((value = g_variant_iter_next_value(&ref_iter)) != NULL) {
        /* helper for being able to auto-free the value */
        g_autoptr(GVariant) child = value;
        const char *ref_name = NULL;
        g_variant_get_child(child, 0, "&s", &ref_name);
        if (ref_name == NULL)
            continue;
        g_autofree char *ref = NULL;
        ostree_parse_refspec(ref_name, NULL, &ref, NULL);
        if (ref == NULL)
            continue;
        // gboolean is_app = g_str_has_prefix(ref, "app/");

        g_autoptr(GVariant) csum_v = NULL;
        char tmp_checksum[65];
        const guchar *csum_bytes;
        g_variant_get_child(child, 1, "(t@aya{sv})", NULL, &csum_v, NULL);
        csum_bytes = ostree_checksum_bytes_peek_validate(csum_v, NULL);
        if (csum_bytes == NULL)
            continue;
        ostree_checksum_inplace_from_bytes(csum_bytes, tmp_checksum);
        // char *newRef = g_new0(char, 1);
        // char* newRef = NULL;
        // newRef = g_strdup(ref);
        // g_hash_table_insert(ret_all_refs, newRef, g_strdup(tmp_checksum));
        outRefs.insert(map<string, string>::value_type(ref, tmp_checksum));
    }
}

/*
 * 从ostree仓库描述文件Summary信息中获取仓库所有软件包索引refs
 *
 * @param summary: 远端仓库Summary信息
 * @param outRefs: 远端仓库软件包索引信息
 */
void RepoHelper::getPkgRefsBySummary(GVariant *summary, map<string, string> &outRefs)
{
    // g_autoptr(GHashTable) ret_all_refs = NULL;
    g_autoptr(GVariant) ref_map = NULL;
    g_autoptr(GVariant) metadata = NULL;

    // ret_all_refs = g_hash_table_new(linglong_collection_ref_hash, linglong_collection_ref_equal);
    ref_map = g_variant_get_child_value(summary, 0);
    metadata = g_variant_get_child_value(summary, 1);
    getPkgRefsFromRefsMap(ref_map, outRefs);
}

/*
 * 查询远端仓库所有软件包索引信息refs
 *
 * @param qrepoPath: 远端仓库对应的本地仓库路径
 * @param qremoteName: 远端仓库名称
 * @param outRefs: 远端仓库软件包索引信息(key:refs, value:commit)
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool RepoHelper::getRemoteRefs(const QString qrepoPath, const QString qremoteName, QMap<QString, QString> &outRefs, QString &err)
{
    const string repoPath = qrepoPath.toStdString();
    const string remoteName = qremoteName.toStdString();
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (remoteName.empty()) {
        // fprintf(stdout, "getRemoteRefs param err\n");
        qInfo() << "getRemoteRefs param err";
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    if (!linglong_dir->repo || linglong_dir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        err = info;
        return false;
    }

    g_autoptr(GBytes) summaryBytes = NULL;
    g_autoptr(GBytes) summarySigBytes = NULL;
    GCancellable *cancellable = NULL;
    GError *error = NULL;
    bool ret = fetchRemoteSummary(linglong_dir->repo, remoteName.c_str(), &summaryBytes, &summarySigBytes, cancellable, &error);
    if (!ret) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s err:%s", __FILE__, __func__, error->message);
        err = info;
        return false;
    }
    GVariant *summary = g_variant_ref_sink(g_variant_new_from_bytes(OSTREE_SUMMARY_GVARIANT_FORMAT, summaryBytes, FALSE));
    // std::map转QMap
    map<string, string> outRet;
    getPkgRefsBySummary(summary, outRet);
    for (auto iter = outRet.begin(); iter != outRet.end(); ++iter) {
        outRefs.insert(QString::fromStdString(iter->first), QString::fromStdString(iter->second));
    }
    return true;
}

/*
 * 按照指定字符分割字符串
 *
 * @param str: 目标字符串
 * @param separator: 分割字符串
 * @param result: 分割结果
 */
void RepoHelper::splitStr(string str, string separator, vector<string> &result)
{
    size_t cutAt;
    while ((cutAt = str.find_first_of(separator)) != str.npos) {
        if (cutAt > 0) {
            result.push_back(str.substr(0, cutAt));
        }
        str = str.substr(cutAt + 1);
    }
    if (str.length() > 0) {
        result.push_back(str);
    }
}

/*
 * 解析仓库软件包索引ref信息
 *
 * @param fullRef: 目标软件包索引ref信息
 * @param result: 解析结果
 *
 * @return bool: true:成功 false:失败
 */
bool RepoHelper::resolveRef(const string fullRef, vector<string> &result)
{
    // vector<string> result;
    splitStr(fullRef, "/", result);
    if (result.size() != 4) {
        // fprintf(stdout, "Wrong number of components in %s", fullRef.c_str());
        qInfo() << "resolveRef Wrong number of components err";
        return false;
    }

    if (result[0] != "app" && result[0] != "runtime") {
        // fprintf(stdout, "%s is not application or runtime", fullRef.c_str());
        qInfo() << "resolveRef application or runtime err";
        return false;
    }

    //   if (!check_valid_name (parts[1], &local_error))
    //     {
    //       fprintf (stdout, "Invalid name %s: %s", parts[1], local_error->message);
    //       return false;
    //     }

    if (result[2].size() == 0) {
        // fprintf(stdout, "Invalid arch %s", result[2].c_str());
        qInfo() << "resolveRef arch err";
        return false;
    }

    //   if (!check_is_valid_branch (parts[3], &local_error))
    //     {
    //     }

    return true;
}

/*
 * 查询软件包在仓库中对应的索引信息ref
 *
 * @param qrepoPath: 远端仓库对应的本地仓库路径
 * @param qremoteName: 远端仓库名称
 * @param qpkgName: 软件包包名
 * @param qarch: 架构名
 * @param matchRef: 软件包对应的索引信息
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool RepoHelper::resolveMatchRefs(const QString qrepoPath, const QString qremoteName, const QString qpkgName, const QString qarch, QString &matchRef, QString &err)
{
    const string repoPath = qrepoPath.toStdString();
    const string remoteName = qremoteName.toStdString();
    const string pkgName = qpkgName.toStdString();
    const string arch = qarch.toStdString();
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (remoteName.empty() || pkgName.empty()) {
        // fprintf(stdout, "resolveMatchRefs param err\n");
        qInfo() << "resolveMatchRefs param err";
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    if (!linglong_dir->repo || linglong_dir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        err = info;
        return false;
    }

    // map<string, string> outRefs;
    // bool ret = getRemoteRefs(repoPath, remoteName, outRefs, err);
    //  if (ret) {
    //      for (auto it = outRefs.begin(); it != outRefs.end(); ++it) {
    //          vector<string> result;
    //          ret = resolveRef(it->first, result);
    //          if (ret && result[1] == pkgName && result[2] == arch) {
    //              matchRef = it->first;
    //              cout << it->first << endl;
    //              cout << it->second << endl;
    //              return true;
    //          }
    //      }
    //  }

    QMap<QString, QString> outRefs;
    bool ret = getRemoteRefs(qrepoPath, qremoteName, outRefs, err);
    if (ret) {
        for (auto it = outRefs.begin(); it != outRefs.end(); ++it) {
            vector<string> result;
            ret = resolveRef(it.key().toStdString(), result);
            // appstream 特殊处理
            if (pkgName == "appstream2" && result[0] == pkgName && result[1] == arch) {
                matchRef = it.key();
                return true;
            }
            // flatpak ref: flathub:app/us.zoom.Zoom/x86_64/stable
            // if (ret && result[1] == pkgName && result[2] == arch) {
            //     matchRef = it.key();
            //     return true;
            // }
            //玲珑适配
            // ref:app/org.deepin.calculator/x86_64/1.2.2
            if (ret && result[1] == pkgName && result[2] == arch) {
                matchRef = it.key();
                return true;
            }
        }
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s pkgName not found", __FILE__, __func__);
        err = info;
    }
    return false;
}

/*
 * 查询ostree pull参数
 *
 * @param builder: ostree pull需要的builder
 * @param ref_to_fetch: 目标软件包索引ref
 * @param dirs_to_pull: pull目标子路径
 * @param current_checksum: 软件包对应的commit值
 * @param force_disable_deltas: 禁止delta设置
 * @param flags: pull参数
 * @param progress: ostree下载进度回调
 *
 */
void RepoHelper::getCommonPullOptions(GVariantBuilder *builder,
                                      const char *ref_to_fetch,
                                      const gchar *const *dirs_to_pull,
                                      const char *current_checksum,
                                      gboolean force_disable_deltas,
                                      OstreeRepoPullFlags flags,
                                      OstreeAsyncProgress *progress)
{
    guint32 update_freq = 0;
    GVariantBuilder hdr_builder;

    if (dirs_to_pull) {
        g_variant_builder_add(builder, "{s@v}", "subdirs",
                              g_variant_new_variant(g_variant_new_strv((const char *const *)dirs_to_pull, -1)));
        force_disable_deltas = TRUE;
    }

    if (force_disable_deltas) {
        g_variant_builder_add(builder, "{s@v}", "disable-static-deltas",
                              g_variant_new_variant(g_variant_new_boolean(TRUE)));
    }

    g_variant_builder_add(builder, "{s@v}", "inherit-transaction",
                          g_variant_new_variant(g_variant_new_boolean(TRUE)));

    g_variant_builder_add(builder, "{s@v}", "flags",
                          g_variant_new_variant(g_variant_new_int32(flags)));

    g_variant_builder_init(&hdr_builder, G_VARIANT_TYPE("a(ss)"));
    g_variant_builder_add(&hdr_builder, "(ss)", "ostree-Ref", ref_to_fetch);
    if (current_checksum)
        g_variant_builder_add(&hdr_builder, "(ss)", "linglong-Upgrade-From", current_checksum);
    g_variant_builder_add(builder, "{s@v}", "http-headers",
                          g_variant_new_variant(g_variant_builder_end(&hdr_builder)));
    g_variant_builder_add(builder, "{s@v}", "append-user-agent",
                          g_variant_new_variant(g_variant_new_string("linglong/" PACKAGE_VERSION)));

    if (progress != NULL)
        update_freq = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(progress), "update-frequency"));
    if (update_freq == 0)
        update_freq = DEFAULT_UPDATE_FREQUENCY;

    g_variant_builder_add(builder, "{s@v}", "update-frequency",
                          g_variant_new_variant(g_variant_new_uint32(update_freq)));
}

/*
 * 获取一个临时cache目录所指的文件路径。
 *
 * @return char*: 临时目录路径
 */
char *RepoHelper::repoReadLink(const char *path)
{
    char buf[PATH_MAX + 1];
    ssize_t symlink_size;

    symlink_size = readlink(path, buf, sizeof(buf) - 1);
    if (symlink_size < 0) {
        // fprintf(stdout, "repoReadLink err\n");
        qInfo() << "repoReadLink err";
        return NULL;
    }
    buf[symlink_size] = 0;
    return g_strdup(buf);
}

/*
 * 获取一个临时cache目录
 *
 * @return char*: 临时目录路径
 */
char *RepoHelper::getCacheDir()
{
    g_autofree char *path = NULL;
    g_autofree char *symlink_path = NULL;
    struct stat st_buf;
    symlink_path = g_build_filename(g_get_user_runtime_dir(), ".linglong-cache", NULL);
    path = repoReadLink(symlink_path);
    if (stat(path, &st_buf) == 0 &&
        /* Must be owned by us */
        st_buf.st_uid == getuid() &&
        /* and not writeable by others */
        (st_buf.st_mode & 0022) == 0)
        // return g_file_new_for_path(path);
        return g_strdup(path);

    path = g_strdup("/var/tmp/linglong-cache-XXXXXX");
    if (g_mkdtemp_full(path, 0755) == NULL) {
        // fprintf(stdout, "Can't create temporary directory\n");
        qInfo() << "Can't create temporary directory";
        return NULL;
    }
    unlink(symlink_path);
    if (symlink(path, symlink_path) != 0) {
        // fprintf(stdout, "symlink err\n");
        qInfo() << "symlink err";
        return NULL;
    }
    return g_strdup(path);
}

/*
 * 删除临时目录
 *
 * @param path: 待删除的路径
 *
 */
void RepoHelper::delDirbyPath(const char *path)
{
    char cmd[512];
    pid_t result;
    memset(cmd, 0, 512);
    sprintf(cmd, "rm -rf %s", path);
    result = system(cmd);
    // fprintf(stdout, "delete tmp repo, path:%s, ret:%d\n", path, result);
    qInfo() << "delete tmp repo";
}

/*
 * 根据指定目录创建临时repo仓库对象
 *
 * @param self: 临时repo仓库对应目标仓库信息
 * @param cache_dir: 临时仓库路径
 * @param error: 错误信息
 *
 * @return OstreeRepo: 临时repo仓库对象
 */
OstreeRepo *RepoHelper::createChildRepo(LingLongDir *self, char *cachePath, GError **error)
{
    g_autoptr(GFile) repo_dir = NULL;
    g_autoptr(GFile) repo_dir_config = NULL;
    OstreeRepo *repo = NULL;
    g_autofree char *tmpdir_name = NULL;
    OstreeRepo *new_repo = NULL;
    g_autoptr(GKeyFile) config = NULL;
    g_autofree char *current_mode = NULL;
    GKeyFile *orig_config = NULL;
    g_autofree char *orig_min_free_space_percent = NULL;
    g_autofree char *orig_min_free_space_size = NULL;

    OstreeRepoMode mode = OSTREE_REPO_MODE_BARE_USER_ONLY;
    const char *mode_str = "bare-user-only";

    // if (!ensureRepoEnv(self, NULL, error))
    //      return NULL;

    orig_config = ostree_repo_get_config(self->repo);

    // g_autofree char *cachePath = g_file_get_path(cache_dir);
    g_autofree char *repoTmpPath = g_strconcat(cachePath, "/repoTmp", NULL);
    // fprintf(stdout, "createChildRepo repoTmpPath:%s\n", repoTmpPath);
    qInfo() << "createChildRepo repoTmpPath:" << repoTmpPath;
    repo_dir = g_file_new_for_path(repoTmpPath);

    if (g_file_query_exists(repo_dir, NULL)) {
        delDirbyPath(repoTmpPath);
    }

    // 创建目录
    if (g_mkdir_with_parents(repoTmpPath, 0755)) {
        // fprintf(stdout, "g_mkdir_with_parents repoTmpPath fialed\n");
        qInfo() << "g_mkdir_with_parents repoTmpPath fialed";
        return NULL;
    }

    new_repo = ostree_repo_new(repo_dir);

    repo_dir_config = g_file_get_child(repo_dir, "config");
    if (!g_file_query_exists(repo_dir_config, NULL)) {
        if (!ostree_repo_create(new_repo, mode, NULL, error))
            return NULL;
    } else {
        /* Try to open, but on failure, re-create */
        if (!ostree_repo_open(new_repo, NULL, NULL)) {
            // rm_rf (repo_dir, NULL, NULL);
            if (!ostree_repo_create(new_repo, mode, NULL, error))
                return NULL;
        }
    }

    config = ostree_repo_copy_config(new_repo);
    /* Verify that the mode is the expected one; if it isn't, recreate the repo */
    current_mode = g_key_file_get_string(config, "core", "mode", NULL);
    if (current_mode == NULL || g_strcmp0(current_mode, mode_str) != 0) {
        // rm_rf (repo_dir, NULL, NULL);

        /* Re-initialize the object because its dir's contents have been deleted (and it
         * holds internal references to them) */
        g_object_unref(new_repo);
        new_repo = ostree_repo_new(repo_dir);

        if (!ostree_repo_create(new_repo, mode, NULL, error))
            return NULL;

        /* Reload the repo config */
        g_key_file_free(config);
        config = ostree_repo_copy_config(new_repo);
    }

    /* Ensure the config is updated */
    g_autofree char *parentRepoPath = g_file_get_path(ostree_repo_get_path(self->repo));
    g_key_file_set_string(config, "core", "parent", parentRepoPath);
    // fprintf(stdout, "createChildRepo parent path:%s\n", parentRepoPath);
    qInfo() << "createChildRepo parent path:" << parentRepoPath;
    /* Copy the min space percent value so it affects the temporary repo too */
    orig_min_free_space_percent = g_key_file_get_value(orig_config, "core", "min-free-space-percent", NULL);
    if (orig_min_free_space_percent)
        g_key_file_set_value(config, "core", "min-free-space-percent", orig_min_free_space_percent);
    /* Copy the min space size value so it affects the temporary repo too */
    orig_min_free_space_size = g_key_file_get_value(orig_config, "core", "min-free-space-size", NULL);
    if (orig_min_free_space_size)
        g_key_file_set_value(config, "core", "min-free-space-size", orig_min_free_space_size);

    if (!ostree_repo_write_config(new_repo, config, error))
        return NULL;

    repo = ostree_repo_new(repo_dir);
    if (!ostree_repo_open(repo, NULL, error))
        return NULL;

    /* We don't need to sync the child repos, they are never used for stable storage, and we
     verify + fsync when importing to stable storage */
    ostree_repo_set_disable_fsync(repo, TRUE);

    /* Create a commitpartial in the child repo to ensure we download everything, because
     any commitpartial state in the parent will not be inherited */
    // if (optional_commit)
    //{
    //    g_autofree char *commitpartial_basename = g_strconcat (optional_commit, ".commitpartial", NULL);
    //    g_autoptr(GFile) commitpartial =
    //    build_file (ostree_repo_get_path (repo),
    //                          "state", commitpartial_basename, NULL);
    //    g_file_replace_contents (commitpartial, "", 0, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL, NULL);
    // }
    return (OstreeRepo *)g_steal_pointer(&repo);
}

/*
 * 创建临时repo仓库对象
 *
 * @param self: 临时repo仓库对应目标仓库信息
 * @param error: 错误信息
 *
 * @return OstreeRepo: 临时repo仓库对象
 */
OstreeRepo *RepoHelper::createTmpRepo(LingLongDir *self, GError **error)
{
    g_autofree char *cache_dir = NULL;
    cache_dir = getCacheDir();
    // fprintf(stdout, "createTmpRepo cache_dir path:%s\n", cache_dir);
    qInfo() << "createTmpRepo cache_dir path:" << cache_dir;
    if (cache_dir == NULL)
        return NULL;
    return createChildRepo(self, cache_dir, error);
}

/*
 * 将软件包数据从远端仓库pull到本地
 *
 * @param qrepoPath: 远端仓库对应的本地仓库路径
 * @param qremoteName: 远端仓库名称
 * @param qpkgName: 软件包包名
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool RepoHelper::repoPull(const QString qrepoPath, const QString qremoteName, const QString qpkgName, QString &err)
{
    const string repoPath = qrepoPath.toStdString();
    const string remoteName = qremoteName.toStdString();
    const string pkgName = qpkgName.toStdString();

    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (remoteName.empty() || pkgName.empty()) {
        // fprintf(stdout, "repoPull param err\n");
        qInfo() << "repoPull param err";
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    if (!linglong_dir->repo || linglong_dir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        err = info;
        return false;
    }

    OstreeRepoPullFlags flags = (OstreeRepoPullFlags)(OSTREE_REPO_PULL_FLAGS_MIRROR | OSTREE_REPO_PULL_FLAGS_BAREUSERONLY_FILES);
    QString qmatchRef = "";
    QString arch = "x86_64";
    bool ret = resolveMatchRefs(qrepoPath, qremoteName, qpkgName, arch, qmatchRef, err);
    if (!ret) {
        return false;
    }
    string matchRef = qmatchRef.toStdString();
    // map<string, string> outRefs;
    // string checksum = outRefs.find(matchRef)->second;
    QMap<QString, QString> outRefs;
    ret = getRemoteRefs(qrepoPath, qremoteName, outRefs, err);
    if (!ret) {
        return false;
    }
    string checksum = outRefs.find(qmatchRef).value().toStdString();
    GCancellable *cancellable = NULL;
    GError *error = NULL;
    OstreeAsyncProgress *progress = ostree_async_progress_new_and_connect(RepoHelper::noProgressFunc, NULL);

    GVariantBuilder builder;
    g_autoptr(GVariant) options = NULL;
    const char *refs[2] = {NULL, NULL};
    const char *commits[2] = {NULL, NULL};

    refs[0] = matchRef.c_str();
    commits[0] = checksum.c_str();

    // current_checksum "ce384fddb07dc50731858f655646da71f93fb6d6d22e9af308a5e69051b4c496"
    // refFetch "app/us.zoom.Zoom/x86_64/stable"
    // remote_name: "flathub"
    // dirs_to_pull: NULL
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    getCommonPullOptions(&builder, matchRef.c_str(), NULL, checksum.c_str(),
                         0, flags, progress);
    g_variant_builder_add(&builder, "{s@v}", "refs",
                          g_variant_new_variant(g_variant_new_strv((const char *const *)refs, -1)));
    g_variant_builder_add(&builder, "{s@v}", "override-commit-ids",
                          g_variant_new_variant(g_variant_new_strv((const char *const *)commits, -1)));
    g_variant_builder_add(&builder, "{s@v}", "override-remote-name",
                          g_variant_new_variant(g_variant_new_string(remoteName.c_str())));
    options = g_variant_ref_sink(g_variant_builder_end(&builder));
    // 下载到临时目录
    // OstreeRepo *childRepo = createTmpRepo(linglong_dir, &error);
    // if (childRepo == NULL) {
    //     //fprintf(stdout, "createTmpRepo error\n");
    //     qInfo() << "createTmpRepo error";
    //     snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s createTmpRepo error err", __FILE__, __func__);
    //     err = info;
    //     return false;
    // }

    if (!ostree_repo_prepare_transaction(linglong_dir->repo, NULL, cancellable, &error)) {
        // fprintf(stdout, "ostree_repo_prepare_transaction error:%s\n", error->message);
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_prepare_transaction err:%s", __FILE__, __func__, error->message);
        err = info;
        return false;
    }
    if (!ostree_repo_pull_with_options(linglong_dir->repo, remoteName.c_str(), options,
                                       progress, cancellable, &error)) {
        // fprintf(stdout, "ostree_repo_pull_with_options error:%s\n", error->message);
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_pull_with_options err:%s", __FILE__, __func__, error->message);
        err = info;
        return false;
    }
    if (progress) {
        ostree_async_progress_finish(progress);
    }
    if (!ostree_repo_commit_transaction(linglong_dir->repo, NULL, cancellable, &error)) {
        // fprintf(stdout, "ostree_repo_commit_transaction error:%s\n", error->message);
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s ostree_repo_commit_transaction err:%s", __FILE__, __func__, error->message);
        err = info;
        return false;
    }

    //将数据从临时目录拷贝到base目录
    // g_autofree char *childRepoPath = g_file_get_path(ostree_repo_get_path(childRepo));
    // g_autofree char *local_url = g_strconcat("file://", childRepoPath, NULL);
    // fprintf(stdout, "local_url:%s\n", local_url);
    // qInfo() << "repoPullLocal local_url:" << local_url;
    // ret = repoPullLocal(linglong_dir->repo, remoteName.c_str(), local_url, matchRef.c_str(), checksum.c_str(), progress, cancellable, &error);
    // if (!ret) {
    //     //fprintf(stdout, "repoPullLocal error:%s\n", error->message);
    //     snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repoPullLocal err:%s", __FILE__, __func__, error->message);
    //     err = info;
    //     return false;
    // }
    // delDirbyPath(childRepoPath);
    return true;
}

/*
 * 将软件包数据从本地临时repo仓库pull到目标repo仓库
 *
 * @param repo: 远端仓库对应的本地仓库OstreeRepo对象
 * @param remoteName: 远端仓库名称
 * @param url: 临时repo仓库对应的url
 * @param ref: 目标软件包索引ref
 * @param checksum: 软件包对应的commit值
 * @param cancellable: GCancellable对象
 * @param error: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool RepoHelper::repoPullLocal(OstreeRepo *repo, const char *remoteName, const char *url, const char *ref, const char *checksum,
                               OstreeAsyncProgress *progress, GCancellable *cancellable, GError **error)
{
    const OstreeRepoPullFlags flags = (OstreeRepoPullFlags)(OSTREE_REPO_PULL_FLAGS_UNTRUSTED | OSTREE_REPO_PULL_FLAGS_BAREUSERONLY_FILES);
    GVariantBuilder builder;
    g_autoptr(GVariant) options = NULL;
    gboolean res;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    const char *refs[2] = {NULL, NULL};
    const char *commits[2] = {NULL, NULL};

    g_autoptr(GError) dummy_error = NULL;

    /* The ostree fetcher asserts if error is NULL */
    if (error == NULL)
        error = &dummy_error;

    g_assert(progress != NULL);

    refs[0] = ref;
    commits[0] = checksum;

    g_variant_builder_add(&builder, "{s@v}", "refs",
                          g_variant_new_variant(g_variant_new_strv((const char *const *)refs, -1)));
    g_variant_builder_add(&builder, "{s@v}", "override-commit-ids",
                          g_variant_new_variant(g_variant_new_strv((const char *const *)commits, -1)));

    g_variant_builder_add(&builder, "{s@v}", "flags",
                          g_variant_new_variant(g_variant_new_int32(flags)));
    g_variant_builder_add(&builder, "{s@v}", "override-remote-name",
                          g_variant_new_variant(g_variant_new_string(remoteName)));
    g_variant_builder_add(&builder, "{s@v}", "gpg-verify",
                          g_variant_new_variant(g_variant_new_boolean(FALSE)));
    g_variant_builder_add(&builder, "{s@v}", "gpg-verify-summary",
                          g_variant_new_variant(g_variant_new_boolean(FALSE)));
    g_variant_builder_add(&builder, "{s@v}", "inherit-transaction",
                          g_variant_new_variant(g_variant_new_boolean(TRUE)));
    g_variant_builder_add(&builder, "{s@v}", "update-frequency",
                          g_variant_new_variant(g_variant_new_uint32(DEFAULT_UPDATE_FREQUENCY)));

    options = g_variant_ref_sink(g_variant_builder_end(&builder));

    if (!ostree_repo_prepare_transaction(repo, NULL, cancellable, error)) {
        // fprintf(stdout, "ostree_repo_prepare_transaction error:%s\n", (*error)->message);
        qInfo() << "ostree_repo_prepare_transaction error:" << (*error)->message;
    }

    res = ostree_repo_pull_with_options(repo, url, options,
                                        progress, cancellable, error);
    if (!res)
        // translate_ostree_repo_pull_errors(error);
        if (progress)
            ostree_async_progress_finish(progress);

    if (!ostree_repo_commit_transaction(repo, NULL, cancellable, error)) {
        // fprintf(stdout, "ostree_repo_commit_transaction error:%s\n", (*error)->message);
        qInfo() << "ostree_repo_commit_transaction error:" << (*error)->message;
    }
    return res;
}

/*
 * 将软件包数据从本地仓库签出到指定目录
 *
 * @param qrepoPath: 远端仓库对应的本地仓库路径
 * @param qremoteName: 远端仓库名称
 * @param qref: 软件包包名对应的仓库索引
 * @param qdstPath: 软件包信息保存目录
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool RepoHelper::checkOutAppData(const QString qrepoPath, const QString qremoteName, const QString qref, const QString qdstPath, QString &err)
{
    const string repoPath = qrepoPath.toStdString();
    const string ref = qref.toStdString();
    const string dstPath = qdstPath.toStdString();

    GCancellable *cancellable = NULL;
    GError *error = NULL;
    OstreeRepoCheckoutAtOptions options = {
        OSTREE_REPO_CHECKOUT_MODE_NONE,
    };
    char info[MAX_ERRINFO_BUFSIZE] = {'\0'};
    if (dstPath.empty() || ref.empty()) {
        // fprintf(stdout, "checkOutAppData param err\n");
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s param err", __FILE__, __func__);
        err = info;
        return false;
    }

    if (!linglong_dir->repo || linglong_dir->basedir != repoPath) {
        snprintf(info, MAX_ERRINFO_BUFSIZE, "%s, function:%s repo has not been created", __FILE__, __func__);
        err = info;
        return false;
    }

    if (g_mkdir_with_parents(dstPath.c_str(), 0755)) {
        // fprintf(stdout, "g_mkdir_with_parents failed\n");
        return false;
    }

    options.mode = OSTREE_REPO_CHECKOUT_MODE_USER;
    options.overwrite_mode = OSTREE_REPO_CHECKOUT_OVERWRITE_UNION_FILES;
    options.enable_fsync = FALSE;
    options.bareuseronly_dirs = TRUE;
    // options.subpath = "/metadata";

    // map<string, string> outRefs;
    // string checksum = outRefs.find(matchRef)->second;
    QMap<QString, QString> outRefs;
    bool ret = getRemoteRefs(qrepoPath, qremoteName, outRefs, err);
    if (!ret) {
        return ret;
    }
    string checksum = outRefs.find(qref).value().toStdString();

    // extract_extra_data (self, checksum, extradir, &created_extra_data, cancellable, error)
    if (!ostree_repo_checkout_at(linglong_dir->repo, &options,
                                 AT_FDCWD, dstPath.c_str(), checksum.c_str(),
                                 cancellable, &error)) {
        fprintf(stdout, "ostree_repo_checkout_at error:%s\n", error->message);
        return false;
    }
    return true;
}
} // namespace linglong