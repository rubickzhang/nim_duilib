// Copyright (c) 2025 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ---------------------------------------------------------------------------
//
// This file was generated by the CEF translator tool and should not edited
// by hand. See the translator.README.txt file in the tools directory for
// more information.
//
// $hash=07a91f8d964e1a4bd64c3817a66ed9969347954d$
//

#ifndef CEF_INCLUDE_CAPI_CEF_PREFERENCE_CAPI_H_
#define CEF_INCLUDE_CAPI_CEF_PREFERENCE_CAPI_H_
#pragma once

#if defined(BUILDING_CEF_SHARED)
#error This file cannot be included DLL-side
#endif

#include "include/capi/cef_base_capi.h"
#include "include/capi/cef_values_capi.h"

#ifdef __cplusplus
extern "C" {
#endif

///
/// Structure that manages custom preference registrations.
///
/// NOTE: This struct is allocated DLL-side.
///
typedef struct _cef_preference_registrar_t {
  ///
  /// Base structure.
  ///
  cef_base_scoped_t base;

  ///
  /// Register a preference with the specified |name| and |default_value|. To
  /// avoid conflicts with built-in preferences the |name| value should contain
  /// an application-specific prefix followed by a period (e.g. "myapp.value").
  /// The contents of |default_value| will be copied. The data type for the
  /// preference will be inferred from |default_value|'s type and cannot be
  /// changed after registration. Returns true (1) on success. Returns false (0)
  /// if |name| is already registered or if |default_value| has an invalid type.
  /// This function must be called from within the scope of the
  /// cef_browser_process_handler_t::OnRegisterCustomPreferences callback.
  ///
  int(CEF_CALLBACK* add_preference)(struct _cef_preference_registrar_t* self,
                                    const cef_string_t* name,
                                    struct _cef_value_t* default_value);
} cef_preference_registrar_t;

///
/// Manage access to preferences. Many built-in preferences are registered by
/// Chromium. Custom preferences can be registered in
/// cef_browser_process_handler_t::OnRegisterCustomPreferences.
///
/// NOTE: This struct is allocated DLL-side.
///
typedef struct _cef_preference_manager_t {
  ///
  /// Base structure.
  ///
  cef_base_ref_counted_t base;

  ///
  /// Returns true (1) if a preference with the specified |name| exists. This
  /// function must be called on the browser process UI thread.
  ///
  int(CEF_CALLBACK* has_preference)(struct _cef_preference_manager_t* self,
                                    const cef_string_t* name);

  ///
  /// Returns the value for the preference with the specified |name|. Returns
  /// NULL if the preference does not exist. The returned object contains a copy
  /// of the underlying preference value and modifications to the returned
  /// object will not modify the underlying preference value. This function must
  /// be called on the browser process UI thread.
  ///
  struct _cef_value_t*(CEF_CALLBACK* get_preference)(
      struct _cef_preference_manager_t* self,
      const cef_string_t* name);

  ///
  /// Returns all preferences as a dictionary. If |include_defaults| is true (1)
  /// then preferences currently at their default value will be included. The
  /// returned object contains a copy of the underlying preference values and
  /// modifications to the returned object will not modify the underlying
  /// preference values. This function must be called on the browser process UI
  /// thread.
  ///
  struct _cef_dictionary_value_t*(CEF_CALLBACK* get_all_preferences)(
      struct _cef_preference_manager_t* self,
      int include_defaults);

  ///
  /// Returns true (1) if the preference with the specified |name| can be
  /// modified using SetPreference. As one example preferences set via the
  /// command-line usually cannot be modified. This function must be called on
  /// the browser process UI thread.
  ///
  int(CEF_CALLBACK* can_set_preference)(struct _cef_preference_manager_t* self,
                                        const cef_string_t* name);

  ///
  /// Set the |value| associated with preference |name|. Returns true (1) if the
  /// value is set successfully and false (0) otherwise. If |value| is NULL the
  /// preference will be restored to its default value. If setting the
  /// preference fails then |error| will be populated with a detailed
  /// description of the problem. This function must be called on the browser
  /// process UI thread.
  ///
  int(CEF_CALLBACK* set_preference)(struct _cef_preference_manager_t* self,
                                    const cef_string_t* name,
                                    struct _cef_value_t* value,
                                    cef_string_t* error);
} cef_preference_manager_t;

///
/// Returns the global preference manager object.
///
CEF_EXPORT cef_preference_manager_t* cef_preference_manager_get_global(void);

#ifdef __cplusplus
}
#endif

#endif  // CEF_INCLUDE_CAPI_CEF_PREFERENCE_CAPI_H_
