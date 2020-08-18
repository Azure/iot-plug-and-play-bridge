// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#include <string>

//
// Functions to convert between string and wstring
//
std::wstring str2wstr(const std::string& str);
std::string wstr2str(const std::wstring& wstr);