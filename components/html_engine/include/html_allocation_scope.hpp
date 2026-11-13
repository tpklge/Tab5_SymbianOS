// SPDX-License-Identifier: MIT
#pragma once
extern "C" bool tab5_html_alloc_scope(bool);
namespace tab5 {
// Route document preparation and retained UI metadata to PSRAM as well as DOM.
struct HtmlAllocationScope {
 bool previous=tab5_html_alloc_scope(true);
 ~HtmlAllocationScope(){tab5_html_alloc_scope(previous);}
 HtmlAllocationScope()=default;
 HtmlAllocationScope(const HtmlAllocationScope&)=delete;
};
}
