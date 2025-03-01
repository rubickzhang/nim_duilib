// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was generated by the CEF translator tool. If making changes by
// hand only do so within the body of existing method and function
// implementations. See the translator.README.txt file in the tools directory
// for more information.
//
// $hash=33730ef63b35f4f142f8e36e305c0c3e8899d910$
//

#include "libcef_dll/ctocpp/post_data_ctocpp.h"

#include <algorithm>

#include "libcef_dll/ctocpp/post_data_element_ctocpp.h"

// STATIC METHODS - Body may be edited by hand.

NO_SANITIZE("cfi-icall") CefRefPtr<CefPostData> CefPostData::Create() {
  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  auto* _retval = cef_post_data_create();

  // Return type: refptr_same
  return CefPostDataCToCpp_Wrap(_retval);
}

// VIRTUAL METHODS - Body may be edited by hand.

NO_SANITIZE("cfi-icall") bool CefPostDataCToCpp::IsReadOnly() {
  auto* _struct = GetStruct();
  if (!_struct->is_read_only) {
    return false;
  }

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  int _retval = _struct->is_read_only(_struct);

  // Return type: bool
  return _retval ? true : false;
}

NO_SANITIZE("cfi-icall") bool CefPostDataCToCpp::HasExcludedElements() {
  auto* _struct = GetStruct();
  if (!_struct->has_excluded_elements) {
    return false;
  }

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  int _retval = _struct->has_excluded_elements(_struct);

  // Return type: bool
  return _retval ? true : false;
}

NO_SANITIZE("cfi-icall") size_t CefPostDataCToCpp::GetElementCount() {
  auto* _struct = GetStruct();
  if (!_struct->get_element_count) {
    return 0;
  }

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  size_t _retval = _struct->get_element_count(_struct);

  // Return type: simple
  return _retval;
}

NO_SANITIZE("cfi-icall")
void CefPostDataCToCpp::GetElements(ElementVector& elements) {
  auto* _struct = GetStruct();
  if (!_struct->get_elements) {
    return;
  }

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Translate param: elements; type: refptr_vec_same_byref
  size_t elementsSize = elements.size();
  size_t elementsCount = std::max(GetElementCount(), elementsSize);
  cef_post_data_element_t** elementsList = NULL;
  if (elementsCount > 0) {
    elementsList = new cef_post_data_element_t*[elementsCount];
    DCHECK(elementsList);
    if (elementsList) {
      memset(elementsList, 0, sizeof(cef_post_data_element_t*) * elementsCount);
    }
    if (elementsList && elementsSize > 0) {
      for (size_t i = 0; i < elementsSize; ++i) {
        elementsList[i] = CefPostDataElementCToCpp_Unwrap(elements[i]);
      }
    }
  }

  // Execute
  _struct->get_elements(_struct, &elementsCount, elementsList);

  // Restore param:elements; type: refptr_vec_same_byref
  elements.clear();
  if (elementsCount > 0 && elementsList) {
    for (size_t i = 0; i < elementsCount; ++i) {
      elements.push_back(CefPostDataElementCToCpp_Wrap(elementsList[i]));
    }
    delete[] elementsList;
  }
}

NO_SANITIZE("cfi-icall")
bool CefPostDataCToCpp::RemoveElement(CefRefPtr<CefPostDataElement> element) {
  auto* _struct = GetStruct();
  if (!_struct->remove_element) {
    return false;
  }

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Verify param: element; type: refptr_same
  DCHECK(element.get());
  if (!element.get()) {
    return false;
  }

  // Execute
  int _retval = _struct->remove_element(
      _struct, CefPostDataElementCToCpp_Unwrap(element));

  // Return type: bool
  return _retval ? true : false;
}

NO_SANITIZE("cfi-icall")
bool CefPostDataCToCpp::AddElement(CefRefPtr<CefPostDataElement> element) {
  auto* _struct = GetStruct();
  if (!_struct->add_element) {
    return false;
  }

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Verify param: element; type: refptr_same
  DCHECK(element.get());
  if (!element.get()) {
    return false;
  }

  // Execute
  int _retval =
      _struct->add_element(_struct, CefPostDataElementCToCpp_Unwrap(element));

  // Return type: bool
  return _retval ? true : false;
}

NO_SANITIZE("cfi-icall") void CefPostDataCToCpp::RemoveElements() {
  auto* _struct = GetStruct();
  if (!_struct->remove_elements) {
    return;
  }

  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING

  // Execute
  _struct->remove_elements(_struct);
}

// CONSTRUCTOR - Do not edit by hand.

CefPostDataCToCpp::CefPostDataCToCpp() {}

// DESTRUCTOR - Do not edit by hand.

CefPostDataCToCpp::~CefPostDataCToCpp() {}

template <>
cef_post_data_t*
CefCToCppRefCounted<CefPostDataCToCpp, CefPostData, cef_post_data_t>::
    UnwrapDerived(CefWrapperType type, CefPostData* c) {
  CHECK(false) << __func__ << " called with unexpected class type " << type;
  return nullptr;
}

template <>
CefWrapperType CefCToCppRefCounted<CefPostDataCToCpp,
                                   CefPostData,
                                   cef_post_data_t>::kWrapperType =
    WT_POST_DATA;
