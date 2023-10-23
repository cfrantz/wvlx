# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load("//rules:repo.bzl", "http_archive_or_local")

def misc_repos(
        lru_cache = None):
    http_archive_or_local(
        name = "com_github_nitnelave_lru_cache",
        local = lru_cache,
        sha256 = "18d7aa96c1905fb3b9d15e29846c609e1b204a5382215a696a63d8f85d383cc8",
        strip_prefix = "lru_cache-48f9cd9f1713ee63172f2a28af6824bbf8161e5c",
        url = "https://github.com/nitnelave/lru_cache/archive/48f9cd9f1713ee63172f2a28af6824bbf8161e5c.tar.gz",
    )
