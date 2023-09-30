// dear imgui, v1.88 WIP
// (tables and columns code)

/*

Index of this file:

// [SECTION] Commentary
// [SECTION] Header mess
// [SECTION] Tables: Main code
// [SECTION] Tables: Simple accessors
// [SECTION] Tables: Row changes
// [SECTION] Tables: Columns changes
// [SECTION] Tables: Columns width management
// [SECTION] Tables: Drawing
// [SECTION] Tables: Sorting
// [SECTION] Tables: Headers
// [SECTION] Tables: Context Menu
// [SECTION] Tables: Settings (.ini data)
// [SECTION] Tables: Garbage Collection
// [SECTION] Tables: Debugging
// [SECTION] Columns, BeginColumns, EndColumns, etc.

*/

// Navigating this file:
// - In Visual Studio IDE: CTRL+comma ("Edit.GoToAll") can follow symbols in comments, whereas CTRL+F12 ("Edit.GoToImplementation") cannot.
// - With Visual Assist installed: ALT+G ("VAssistX.GoToImplementation") can also follow symbols in comments.

//-----------------------------------------------------------------------------
// [SECTION] Commentary
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Typical tables call flow: (root level is generally public API):
//-----------------------------------------------------------------------------
// - BeginTable()                               user begin into a table
//    | BeginChild()                            - (if ScrollX/ScrollY is set)
//    | TableBeginInitMemory()                  - first time table is used
//    | TableResetSettings()                    - on settings reset
//    | TableLoadSettings()                     - on settings load
//    | TableBeginApplyRequests()               - apply queued resizing/reordering/hiding requests
//    | - TableSetColumnWidth()                 - apply resizing width (for mouse resize, often requested by previous frame)
//    |    - TableUpdateColumnsWeightFromWidth()- recompute columns weights (of stretch columns) from their respective width
// - TableSetupColumn()                         user submit columns details (optional)
// - TableSetupScrollFreeze()                   user submit scroll freeze information (optional)
//-----------------------------------------------------------------------------
// - TableUpdateLayout() [Internal]             followup to BeginTable(): setup everything: widths, columns positions, clipping rectangles. Automatically called by the FIRST call to TableNextRow() or TableHeadersRow().
//    | TableSetupDrawChannels()                - setup ImDrawList channels
//    | TableUpdateBorders()                    - detect hovering columns for resize, ahead of contents submission
//    | TableDrawContextMenu()                  - draw right-click context menu
//-----------------------------------------------------------------------------
// - TableHeadersRow() or TableHeader()         user submit a headers row (optional)
//    | TableSortSpecsClickColumn()             - when left-clicked: alter sort order and sort direction
//    | TableOpenContextMenu()                  - when right-clicked: trigger opening of the default context menu
// - TableGetSortSpecs()                        user queries updated sort specs (optional, generally after submitting headers)
// - TableNextRow()                             user begin into a new row (also automatically called by TableHeadersRow())
//    | TableEndRow()                           - finish existing row
//    | TableBeginRow()                         - add a new row
// - TableSetColumnIndex() / TableNextColumn()  user begin into a cell
//    | TableEndCell()                          - close existing column/cell
//    | TableBeginCell()                        - enter into current column/cell
// - [...]                                      user emit contents
//-----------------------------------------------------------------------------
// - EndTable()                                 user ends the table
//    | TableDrawBorders()                      - draw outer borders, inner vertical borders
//    | TableMergeDrawChannels()                - merge draw channels if clipping isn't required
//    | EndChild()                              - (if ScrollX/ScrollY is set)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// TABLE SIZING
//-----------------------------------------------------------------------------
// (Read carefully because this is subtle but it does make sense!)
//-----------------------------------------------------------------------------
// About 'outer_size':
// Its meaning needs to differ slightly depending on if we are using ScrollX/ScrollY flags.
// Default value is ImVec2(0.0f, 0.0f).
//   X
//   - outer_size.x <= 0.0f  ->  Right-align from window/work-rect right-most edge. With -FLT_MIN or 0.0f will align exactly on right-most edge.
//   - outer_size.x  > 0.0f  ->  Set Fixed width.
//   Y with ScrollX/ScrollY disabled: we output table directly in current window
//   - outer_size.y  < 0.0f  ->  Bottom-align (but will auto extend, unless _NoHostExtendY is set). Not meaningful is parent window can vertically scroll.
//   - outer_size.y  = 0.0f  ->  No minimum height (but will auto extend, unless _NoHostExtendY is set)
//   - outer_size.y  > 0.0f  ->  Set Minimum height (but will auto extend, unless _NoHostExtenY is set)
//   Y with ScrollX/ScrollY enabled: using a child window for scrolling
//   - outer_size.y  < 0.0f  ->  Bottom-align. Not meaningful is parent window can vertically scroll.
//   - outer_size.y  = 0.0f  ->  Bottom-align, consistent with BeginChild(). Not recommended unless table is last item in parent window.
//   - outer_size.y  > 0.0f  ->  Set Exact height. Recommended when using Scrolling on any axis.
//-----------------------------------------------------------------------------
// Outer size is also affected by the NoHostExtendX/NoHostExtendY flags.
// Important to that note how the two flags have slightly different behaviors!
//   - ImGuiTableFlags_NoHostExtendX -> Make outer width auto-fit to columns (overriding outer_size.x value). Only available when ScrollX/ScrollY are disabled and Stretch columns are not used.
//   - ImGuiTableFlags_NoHostExtendY -> Make outer height stop exactly at outer_size.y (prevent auto-extending table past the limit). Only available when ScrollX/ScrollY is disabled. Data below the limit will be clipped and not visible.
// In theory ImGuiTableFlags_NoHostExtendY could be the default and any non-scrolling tables with outer_size.y != 0.0f would use exact height.
// This would be consistent but perhaps less useful and more confusing (as vertically clipped items are not easily noticeable)
//-----------------------------------------------------------------------------
// About 'inner_width':
//   With ScrollX disabled:
//   - inner_width          ->  *ignored*
//   With ScrollX enabled:
//   - inner_width  < 0.0f  ->  *illegal* fit in known width (right align from outer_size.x) <-- weird
//   - inner_width  = 0.0f  ->  fit in outer_width: Fixed size columns will take space they need (if avail, otherwise shrink down), Stretch columns becomes Fixed columns.
//   - inner_width  > 0.0f  ->  override scrolling width, generally to be larger than outer_size.x. Fixed column take space they need (if avail, otherwise shrink down), Stretch columns share remaining space!
//-----------------------------------------------------------------------------
// Details:
// - If you want to use Stretch columns with ScrollX, you generally need to specify 'inner_width' otherwise the concept
//   of "available space" doesn't make sense.
// - Even if not really useful, we allow 'inner_width < outer_size.x' for consistency and to facilitate understanding
//   of what the value does.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// COLUMNS SIZING POLICIES
//-----------------------------------------------------------------------------
// About overriding column sizing policy and width/weight with TableSetupColumn():
// We use a default parameter of 'init_width_or_weight == -1'.
//   - with ImGuiTableColumnFlags_WidthFixed,    init_width  <= 0 (default)  --> width is automatic
//   - with ImGuiTableColumnFlags_WidthFixed,    init_width  >  0 (explicit) --> width is custom
//   - with ImGuiTableColumnFlags_WidthStretch,  init_weight <= 0 (default)  --> weight is 1.0f
//   - with ImGuiTableColumnFlags_WidthStretch,  init_weight >  0 (explicit) --> weight is custom
// Widths are specified _without_ CellPadding. If you specify a width of 100.0f, the column will be cover (100.0f + Padding * 2.0f)
// and you can fit a 100.0f wide item in it without clipping and with full padding.
//-----------------------------------------------------------------------------
// About default sizing policy (if you don't specify a ImGuiTableColumnFlags_WidthXXXX flag)
//   - with Table policy ImGuiTableFlags_SizingFixedFit      --> default Column policy is ImGuiTableColumnFlags_WidthFixed, default Width is equal to contents width
//   - with Table policy ImGuiTableFlags_SizingFixedSame     --> default Column policy is ImGuiTableColumnFlags_WidthFixed, default Width is max of all contents width
//   - with Table policy ImGuiTableFlags_SizingStretchSame   --> default Column policy is ImGuiTableColumnFlags_WidthStretch, default Weight is 1.0f
//   - with Table policy ImGuiTableFlags_SizingStretchWeight --> default Column policy is ImGuiTableColumnFlags_WidthStretch, default Weight is proportional to contents
// Default Width and default Weight can be overridden when calling TableSetupColumn().
//-----------------------------------------------------------------------------
// About mixing Fixed/Auto and Stretch columns together:
//   - the typical use of mixing sizing policies is: any number of LEADING Fixed columns, followed by one or two TRAILING Stretch columns.
//   - using mixed policies with ScrollX does not make much sense, as using Stretch columns with ScrollX does not make much sense in the first place!
//     that is, unless 'inner_width' is passed to BeginTable() to explicitly provide a total width to layout columns in.
//   - when using ImGuiTableFlags_SizingFixedSame with mixed columns, only the Fixed/Auto columns will match their widths to the width of the maximum contents.
//   - when using ImGuiTableFlags_SizingStretchSame with mixed columns, only the Stretch columns will match their weight/widths.
//-----------------------------------------------------------------------------
// About using column width:
// If a column is manual resizable or has a width specified with TableSetupColumn():
//   - you may use GetContentRegionAvail().x to query the width available in a given column.
//   - right-side alignment features such as SetNextItemWidth(-x) or PushItemWidth(-x) will rely on this width.
// If the column is not resizable and has no width specified with TableSetupColumn():
//   - its width will be automatic and be set to the max of items submitted.
//   - therefore you generally cannot have ALL items of the columns use e.g. SetNextItemWidth(-FLT_MIN).
//   - but if the column has one or more items of known/fixed size, this will become the reference width used by SetNextItemWidth(-FLT_MIN).
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// TABLES CLIPPING/CULLING
//-----------------------------------------------------------------------------
// About clipping/culling of Rows in Tables:
// - For large numbers of rows, it is recommended you use ImGuiListClipper to only submit visible rows.
//   ImGuiListClipper is reliant on the fact that rows are of equal height.
//   See 'Demo->Tables->Vertical Scrolling' or 'Demo->Tables->Advanced' for a demo of using the clipper.
// - Note that auto-resizing columns don't play well with using the clipper.
//   By default a table with _ScrollX but without _Resizable will have column auto-resize.
//   So, if you want to use the clipper, make sure to either enable _Resizable, either setup columns width explicitly with _WidthFixed.
//-----------------------------------------------------------------------------
// About clipping/culling of Columns in Tables:
// - Both TableSetColumnIndex() and TableNextColumn() return true when the column is visible or performing
//   width measurements. Otherwise, you may skip submitting the contents of a cell/column, BUT ONLY if you know
//   it is not going to contribute to row height.
//   In many situations, you may skip submitting contents for every column but one (e.g. the first one).
// - Case A: column is not hidden by user, and at least partially in sight (most common case).
// - Case B: column is clipped / out of sight (because of scrolling or parent ClipRect): TableNextColumn() return false as a hint but we still allow layout output.
// - Case C: column is hidden explicitly by the user (e.g. via the context menu, or _DefaultHide column flag, etc.).
//
//                        [A]         [B]          [C]
//  TableNextColumn():    true        false        false       -> [userland] when TableNextColumn() / TableSetColumnIndex() return false, user can skip submitting items but only if the column doesn't contribute to row height.
//          SkipItems:    false       false        true        -> [internal] when SkipItems is true, most widgets will early out if submitted, resulting is no layout output.
//           ClipRect:    normal      zero-width   zero-width  -> [internal] when ClipRect is zero, ItemAdd() will return false and most widgets will early out mid-way.
//  ImDrawList output:    normal      dummy        dummy       -> [internal] when using the dummy channel, ImDrawList submissions (if any) will be wasted (because cliprect is zero-width anyway).
//
// - We need to distinguish those cases because non-hidden columns that are clipped outside of scrolling bounds should still contribute their height to the row.
//   However, in the majority of cases, the contribution to row height is the same for all columns, or the tallest cells are known by the programmer.
//-----------------------------------------------------------------------------
// About clipping/culling of whole Tables:
// - Scrolling tables with a known outer size can be clipped earlier as BeginTable() will return false.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// [SECTION] Header mess
//-----------------------------------------------------------------------------

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "imgui.h"
#ifndef IMGUI_DISABLE

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

// System includes
#if defined(_MSC_VER) && _MSC_VER <= 1500 // MSVC 2008 or earlier
#include <stddef.h>     // intptr_t
#else
#include <stdint.h>     // intptr_t
#endif

// Visual Studio warnings
#ifdef _MSC_VER
#pragma warning (disable: 4127)     // condition expression is constant
#pragma warning (disable: 4996)     // 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#if defined(_MSC_VER) && _MSC_VER >= 1922 // MSVC 2019 16.2 or later
#pragma warning (disable: 5054)     // operator '|': deprecated between enumerations of different types
#endif
#pragma warning (disable: 26451)    // [Static Analyzer] Arithmetic overflow : Using operator 'xxx' on a 4 byte value and then casting the result to a 8 byte value. Cast the value to the wider type before calling operator 'xxx' to avoid overflow(io.2).
#pragma warning (disable: 26812)    // [Static Analyzer] The enum type 'xxx' is unscoped. Prefer 'enum class' over 'enum' (Enum.3).
#endif

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#if __has_warning("-Wunknown-warning-option")
#pragma clang diagnostic ignored "-Wunknown-warning-option"         // warning: unknown warning group 'xxx'                      // not all warnings are known by all Clang versions and they tend to be rename-happy.. so ignoring warnings triggers new warnings on some configuration. Great!
#endif
#pragma clang diagnostic ignored "-Wunknown-pragmas"                // warning: unknown warning group 'xxx'
#pragma clang diagnostic ignored "-Wold-style-cast"                 // warning: use of old-style cast                            // yes, they are more terse.
#pragma clang diagnostic ignored "-Wfloat-equal"                    // warning: comparing floating point with == or != is unsafe // storing and comparing against same constants (typically 0.0f) is ok.
#pragma clang diagnostic ignored "-Wformat-nonliteral"              // warning: format string is not a string literal            // passing non-literal to vsnformat(). yes, user passing incorrect format strings can crash the code.
#pragma clang diagnostic ignored "-Wsign-conversion"                // warning: implicit conversion changes signedness
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"  // warning: zero as null pointer constant                    // some standard header variations use #define NULL 0
#pragma clang diagnostic ignored "-Wdouble-promotion"               // warning: implicit conversion from 'float' to 'double' when passing argument to function  // using printf() is a misery with this as C++ va_arg ellipsis changes float to double.
#pragma clang diagnostic ignored "-Wenum-enum-conversion"           // warning: bitwise operation between different enumeration types ('XXXFlags_' and 'XXXFlagsPrivate_')
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"// warning: bitwise operation between different enumeration types ('XXXFlags_' and 'XXXFlagsPrivate_') is deprecated
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"  // warning: implicit conversion from 'xxx' to 'float' may lose precision
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wpragmas"                          // warning: unknown option after '#pragma GCC diagnostic' kind
#pragma GCC diagnostic ignored "-Wformat-nonliteral"                // warning: format not a string literal, format string not checked
#pragma GCC diagnostic ignored "-Wclass-memaccess"                  // [__GNUC__ >= 8] warning: 'memset/memcpy' clearing/writing an object of type 'xxxx' with no trivial copy-assignment; use assignment or value-initialization instead
#endif

//-----------------------------------------------------------------------------
// [SECTION] Tables: Main code
//-----------------------------------------------------------------------------
// - TableFixFlags() [Internal]
// - TableFindByID() [Internal]
// - BeginTable()
// - BeginTableEx() [Internal]
// - TableBeginInitMemory() [Internal]
// - TableBeginApplyRequests() [Internal]
// - TableSetupColumnFlags() [Internal]
// - TableUpdateLayout() [Internal]
// - TableUpdateBorders() [Internal]
// - EndTable()
// - TableSetupColumn()
// - TableSetupScrollFreeze()
//-----------------------------------------------------------------------------

// Configuration
static const int TABLE_DRAW_CHANNEL_BG0 = 0;
static const int TABLE_DRAW_CHANNEL_BG2_FROZEN = 1;
static const int TABLE_DRAW_CHANNEL_NOCLIP = 2;                     // When using ImGuiTableFlags_NoClip (this becomes the last visible channel)
static const float TABLE_BORDER_SIZE = 1.0f;    // FIXME-TABLE: Currently hard-coded because of clipping assumptions with outer borders rendering.
static const float TABLE_RESIZE_SEPARATOR_HALF_THICKNESS = 4.0f;    // Extend outside inner borders.
static const float TABLE_RESIZE_SEPARATOR_FEEDBACK_TIMER = 0.06f;   // Delay/timer before making the hover feedback (color+cursor) visible because tables/columns tends to be more cramped.

// Helper
inline ImGuiTableFlags TableFixFlags(ImGuiTableFlags flags, ImGuiWindow* outer_window)
{
    // Adjust flags: set default sizing policy
    if ((flags & ImGuiTableFlags_SizingMask_) == 0)
        flags |= ((flags & ImGuiTableFlags_ScrollX) || (outer_window->Flags & ImGuiWindowFlags_AlwaysAutoResize)) ? ImGuiTableFlags_SizingFixedFit : ImGuiTableFlags_SizingStretchSame;

    // Adjust flags: enable NoKeepColumnsVisible when using ImGuiTableFlags_SizingFixedSame
    if ((flags & ImGuiTableFlags_SizingMask_) == ImGuiTableFlags_SizingFixedSame)
        flags |= ImGuiTableFlags_NoKeepColumnsVisible;

    // Adjust flags: enforce borders when resizable
    if (flags & ImGuiTableFlags_Resizable)
        flags |= ImGuiTableFlags_BordersInnerV;

    // Adjust flags: disable NoHostExtendX/NoHostExtendY if we have any scrolling going on
    if (flags & (ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY))
        flags &= ~(ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_NoHostExtendY);

    // Adjust flags: NoBordersInBodyUntilResize takes priority over NoBordersInBody
    if (flags & ImGuiTableFlags_NoBordersInBodyUntilResize)
        flags &= ~ImGuiTableFlags_NoBordersInBody;

    // Adjust flags: disable saved settings if there's nothing to save
    if ((flags & (ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable)) == 0)
        flags |= ImGuiTableFlags_NoSavedSettings;

    // Inherit _NoSavedSettings from top-level window (child windows always have _NoSavedSettings set)
    if (outer_window->RootWindow->Flags & ImGuiWindowFlags_NoSavedSettings)
        flags |= ImGuiTableFlags_NoSavedSettings;

    return flags;
}

ImGuiTable* ImGui::TableFindByID(ImGuiID id)
{
    ImGuiContext& g = *GImGui;
    return g.Tables.GetByKey(id);
}

// Read about "TABLE SIZING" at the top of this file.
bool    ImGui::BeginTable(const char* str_id, int columns_count, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width)
{
    ImGuiID id = GetID(str_id);
    return BeginTableEx(str_id, id, columns_count, flags, outer_size, inner_width);
}

bool    ImGui::BeginTableEx(const char* name, ImGuiID id, int columns_count, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* outer_window = GetCurrentWindow();
    if (outer_window->SkipItems) // Consistent with other tables + beneficial side effect that assert on miscalling EndTable() will be more visible.
        return false;

    // Sanity checks
    IM_ASSERT(columns_count > 0 && columns_count <= IMGUI_TABLE_MAX_COLUMNS && "Only 1..64 columns allowed!");
    if (flags & ImGuiTableFlags_ScrollX)
        IM_ASSERT(inner_width >= 0.0f);

    // If an outer size is specified ahead we will be able to early out when not visible. Exact clipping rules may evolve.
    const bool use_child_window = (flags & (ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY)) != 0;
    const ImVec2 avail_size = GetContentRegionAvail();
    ImVec2 actual_outer_size = CalcItemSize(outer_size, ImMax(avail_size.x, 1.0f), use_child_window ? ImMax(avail_size.y, 1.0f) : 0.0f);
    ImRect outer_rect(outer_window->DC.CursorPos, outer_window->DC.CursorPos + actual_outer_size);
    if (use_child_window && IsClippedEx(outer_rect, 0))
    {
        ItemSize(outer_rect);
        return false;
    }

    // Acquire storage for the table
    ImGuiTable* table = g.Tables.GetOrAddByKey(id);
    const int instance_no = (table->LastFrameActive != g.FrameCount) ? 0 : table->InstanceCurrent + 1;
    const ImGuiID instance_id = id + instance_no;
    const ImGuiTableFlags table_last_flags = table->Flags;
    if (instance_no > 0)
        IM_ASSERT(table->ColumnsCount == columns_count && "BeginTable(): Cannot change columns count mid-frame while preserving same ID");

    // Acquire temporary buffers
    const int table_idx = g.Tables.GetIndex(table);
    if (++g.TablesTempDataStacked > g.TablesTempData.Size)
        g.TablesTempData.resize(g.TablesTempDataStacked, ImGuiTableTempData());
    ImGuiTableTempData* temp_data = table->TempData = &g.TablesTempData[g.TablesTempDataStacked - 1];
    temp_data->TableIndex = table_idx;
    table->DrawSplitter = &table->TempData->DrawSplitter;
    table->DrawSplitter->Clear();

    // Fix flags
    table->IsDefaultSizingPolicy = (flags & ImGuiTableFlags_SizingMask_) == 0;
    flags = TableFixFlags(flags, outer_window);

    // Initialize
    table->ID = id;
    table->Flags = flags;
    table->InstanceCurrent = (ImS16)instance_no;
    table->LastFrameActive = g.FrameCount;
    table->OuterWindow = table->InnerWindow = outer_window;
    table->ColumnsCount = columns_count;
    table->IsLayoutLocked = false;
    table->InnerWidth = inner_width;
    temp_data->UserOuterSize = outer_size;
    if (instance_no > 0 && table->InstanceDataExtra.Size < instance_no)
        table->InstanceDataExtra.push_back(ImGuiTableInstanceData());

    // When not using a child window, WorkRect.Max will grow as we append contents.
    if (use_child_window)
    {
        // Ensure no vertical scrollbar appears if we only want horizontal one, to make flag consistent
        // (we have no other way to disable vertical scrollbar of a window while keeping the horizontal one showing)
        ImVec2 override_content_size(FLT_MAX, FLT_MAX);
        if ((flags & ImGuiTableFlags_ScrollX) && !(flags & ImGuiTableFlags_ScrollY))
            override_content_size.y = FLT_MIN;

        // Ensure specified width (when not specified, Stretched columns will act as if the width == OuterWidth and
        // never lead to any scrolling). We don't handle inner_width < 0.0f, we could potentially use it to right-align
        // based on the right side of the child window work rect, which would require knowing ahead if we are going to
        // have decoration taking horizontal spaces (typically a vertical scrollbar).
        if ((flags & ImGuiTableFlags_ScrollX) && inner_width > 0.0f)
            override_content_size.x = inner_width;

        if (override_content_size.x != FLT_MAX || override_content_size.y != FLT_MAX)
            SetNextWindowContentSize(ImVec2(override_content_size.x != FLT_MAX ? override_content_size.x : 0.0f, override_content_size.y != FLT_MAX ? override_content_size.y : 0.0f));

        // Reset scroll if we are reactivating it
        if ((table_last_flags & (ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY)) == 0)
            SetNextWindowScroll(ImVec2(0.0f, 0.0f));

        // Create scrolling region (without border and zero window padding)
        ImGuiWindowFlags child_flags = (flags & ImGuiTableFlags_ScrollX) ? ImGuiWindowFlags_HorizontalScrollbar : ImGuiWindowFlags_None;
        BeginChildEx(name, instance_id, outer_rect.GetSize(), false, child_flags);
        table->InnerWindow = g.CurrentWindow;
        table->WorkRect = table->InnerWindow->WorkRect;
        table->OuterRect = table->InnerWindow->Rect();
        table->InnerRect = table->InnerWindow->InnerRect;
        IM_ASSERT(table->InnerWindow->WindowPadding.x == 0.0f && table->InnerWindow->WindowPadding.y == 0.0f && table->InnerWindow->WindowBorderSize == 0.0f);
    }
    else
    {
        // For non-scrolling tables, WorkRect == OuterRect == InnerRect.
        // But at this point we do NOT have a correct value for .Max.y (unless a height has been explicitly passed in). It will only be updated in EndTable().
        table->WorkRect = table->OuterRect = table->InnerRect = outer_rect;
    }

    // Push a standardized ID for both child-using and not-child-using tables
    PushOverrideID(instance_id);

    // Backup a copy of host window members we will modify
    ImGuiWindow* inner_window = table->InnerWindow;
    table->HostIndentX = inner_window->DC.Indent.x;
    table->HostClipRect = inner_window->ClipRect;
    table->HostSkipItems = inner_window->SkipItems;
    temp_data->HostBackupWorkRect = inner_window->WorkRect;
    temp_data->HostBackupParentWorkRect = inner_window->ParentWorkRect;
    temp_data->HostBackupColumnsOffset = outer_window->DC.ColumnsOffset;
    temp_data->HostBackupPrevLineSize = inner_window->DC.PrevLineSize;
    temp_data->HostBackupCurrLineSize = inner_window->DC.CurrLineSize;
    temp_data->HostBackupCursorMaxPos = inner_window->DC.CursorMaxPos;
    temp_data->HostBackupItemWidth = outer_window->DC.ItemWidth;
    temp_data->HostBackupItemWidthStackSize = outer_window->DC.ItemWidthStack.Size;
    inner_window->DC.PrevLineSize = inner_window->DC.CurrLineSize = ImVec2(0.0f, 0.0f);

    // Padding and Spacing
    // - None               ........Content..... Pad .....Content........
    // - PadOuter           | Pad ..Content..... Pad .....Content.. Pad |
    // - PadInner           ........Content.. Pad | Pad ..Content........
    // - PadOuter+PadInner  | Pad ..Content.. Pad | Pad ..Content.. Pad |
    const bool pad_outer_x = (flags & ImGuiTableFlags_NoPadOuterX) ? false : (flags & ImGuiTableFlags_PadOuterX) ? true : (flags & ImGuiTableFlags_BordersOuterV) != 0;
    const bool pad_inner_x = (flags & ImGuiTableFlags_NoPadInnerX) ? false : true;
    const float inner_spacing_for_border = (flags & ImGuiTableFlags_BordersInnerV) ? TABLE_BORDER_SIZE : 0.0f;
    const float inner_spacing_explicit = (pad_inner_x && (flags & ImGuiTableFlags_BordersInnerV) == 0) ? g.Style.CellPadding.x : 0.0f;
    const float inner_padding_explicit = (pad_inner_x && (flags & ImGuiTableFlags_BordersInnerV) != 0) ? g.Style.CellPadding.x : 0.0f;
    table->CellSpacingX1 = inner_spacing_explicit + inner_spacing_for_border;
    table->CellSpacingX2 = inner_spacing_explicit;
    table->CellPaddingX = inner_padding_explicit;
    table->CellPaddingY = g.Style.CellPadding.y;

    const float outer_padding_for_border = (flags & ImGuiTableFlags_BordersOuterV) ? TABLE_BORDER_SIZE : 0.0f;
    const float outer_padding_explicit = pad_outer_x ? g.Style.CellPadding.x : 0.0f;
    table->OuterPaddingX = (outer_padding_for_border + outer_padding_explicit) - table->CellPaddingX;

    table->CurrentColumn = -1;
    table->CurrentRow = -1;
    table->RowBgColorCounter = 0;
    table->LastRowFlags = ImGuiTableRowFlags_None;
    table->InnerClipRect = (inner_window == outer_window) ? table->WorkRect : inner_window->ClipRect;
    table->InnerClipRect.ClipWith(table->WorkRect);     // We need this to honor inner_width
    table->InnerClipRect.ClipWithFull(table->HostClipRect);
    table->InnerClipRect.Max.y = (flags & ImGuiTableFlags_NoHostExtendY) ? ImMin(table->InnerClipRect.Max.y, inner_window->WorkRect.Max.y) : inner_window->ClipRect.Max.y;

    table->RowPosY1 = table->RowPosY2 = table->WorkRect.Min.y; // This is needed somehow
    table->RowTextBaseline = 0.0f; // This will be cleared again by TableBeginRow()
    table->FreezeRowsRequest = table->FreezeRowsCount = 0; // This will be setup by TableSetupScrollFreeze(), if any
    table->FreezeColumnsRequest = table->FreezeColumnsCount = 0;
    table->IsUnfrozenRows = true;
    table->DeclColumnsCount = 0;

    // Using opaque colors facilitate overlapping elements of the grid
    table->BorderColorStrong = GetColorU32(ImGuiCol_TableBorderStrong);
    table->BorderColorLight = GetColorU32(ImGuiCol_TableBorderLight);

    // Make table current
    g.CurrentTable = table;
    outer_window->DC.CurrentTableIdx = table_idx;
    if (inner_window != outer_window) // So EndChild() within the inner window can restore the table properly.
        inner_window->DC.CurrentTableIdx = table_idx;

    if ((table_last_flags & ImGuiTableFlags_Reorderable) && (flags & ImGuiTableFlags_Reorderable) == 0)
        table->IsResetDisplayOrderRequest = true;

    // Mark as used
    if (table_idx >= g.TablesLastTimeActive.Size)
        g.TablesLastTimeActive.resize(table_idx + 1, -1.0f);
    g.TablesLastTimeActive[table_idx] = (float)g.Time;
    temp_data->LastTimeActive = (float)g.Time;
    table->MemoryCompacted = false;

    // Setup memory buffer (clear data if columns count changed)
    ImGuiTableColumn* old_columns_to_preserve = NULL;
    void* old_columns_raw_data = NULL;
    const int old_columns_count = table->Columns.size();
    if (old_columns_count != 0 && old_columns_count != columns_count)
    {
        // Attempt to preserve width on column count change (#4046)
        old_columns_to_preserve = table->Columns.Data;
        old_columns_raw_data = table->RawData;
        table->RawData = NULL;
    }
    if (table->RawData == NULL)
    {
        TableBeginInitMemory(table, columns_count);
        table->IsInitializing = table->IsSettingsRequestLoad = true;
    }
    if (table->IsResetAllRequest)
        TableResetSettings(table);
    if (table->IsInitializing)
    {
        // Initialize
        table->SettingsOffset = -1;
        table->IsSortSpecsDirty = true;
        table->InstanceInteracted = -1;
        table->ContextPopupColumn = -1;
        table->ReorderColumn = table->ResizedColumn = table->LastResizedColumn = -1;
        table->AutoFitSingleColumn = -1;
        table->HoveredColumnBody = table->HoveredColumnBorder = -1;
        for (int n = 0; n < columns_count; n++)
        {
            ImGuiTableColumn* column = &table->Columns[n];
            if (old_columns_to_preserve && n < old_columns_count)
            {
                // FIXME: We don't attempt to preserve column order in this path.
                *column = old_columns_to_preserve[n];
            }
            else
            {
                float width_auto = column->WidthAuto;
                *column = ImGuiTableColumn();
                column->WidthAuto = width_auto;
                column->IsPreserveWidthAuto = true; // Preserve WidthAuto when reinitializing a live table: not technically necessary but remove a visible flicker
                column->IsEnabled = column->IsUserEnabled = column->IsUserEnabledNextFrame = true;
            }
            column->DisplayOrder = table->DisplayOrderToIndex[n] = (ImGuiTableColumnIdx)n;
        }
    }
    if (old_columns_raw_data)
        IM_FREE(old_columns_raw_data);

    // Load settings
    if (table->IsSettingsRequestLoad)
        TableLoadSettings(table);

    // Handle DPI/font resize
    // This is designed to facilitate DPI changes with the assumption that e.g. style.CellPadding has been scaled as well.
    // It will also react to changing fonts with mixed results. It doesn't need to be perfect but merely provide a decent transition.
    // FIXME-DPI: Provide consistent standards for reference size. Perhaps using g.CurrentDpiScale would be more self explanatory.
    // This is will lead us to non-rounded WidthRequest in columns, which should work but is a poorly tested path.
    const float new_ref_scale_unit = g.FontSize; // g.Font->GetCharAdvance('A') ?
    if (table->RefScale != 0.0f && table->RefScale != new_ref_scale_unit)
    {
        const float scale_factor = new_ref_scale_unit / table->RefScale;
        //IMGUI_DEBUG_LOG("[table] %08X RefScaleUnit %.3f -> %.3f, scaling width by %.3f\n", table->ID, table->RefScaleUnit, new_ref_scale_unit, scale_factor);
        for (int n = 0; n < columns_count; n++)
            table->Columns[n].WidthRequest = table->Columns[n].WidthRequest * scale_factor;
    }
    table->RefScale = new_ref_scale_unit;

    // Disable output until user calls TableNextRow() or TableNextColumn() leading to the TableUpdateLayout() call..
    // This is not strictly necessary but will reduce cases were "out of table" output will be misleading to the user.
    // Because we cannot safely assert in EndTable() when no rows have been created, this seems like our best option.
    inner_window->SkipItems = true;

    // Clear names
    // At this point the ->NameOffset field of each column will be invalid until TableUpdateLayout() or the first call to TableSetupColumn()
    if (table->ColumnsNames.Buf.Size > 0)
        table->ColumnsNames.Buf.resize(0);

    // Apply queued resizing/reordering/hiding requests
    TableBeginApplyRequests(table);

    return true;
}

// For reference, the average total _allocation count_ for a table is:
// + 0 (for ImGuiTable instance, we are pooling allocations in g.Tables)
// + 1 (for table->RawData allocated below)
// + 1 (for table->ColumnsNames, if names are used)
// Shared allocations per number of nested tables
// + 1 (for table->Splitter._Channels)
// + 2 * active_channels_count (for ImDrawCmd and ImDrawIdx buffers inside channels)
// Where active_channels_count is variable but often == columns_count or columns_count + 1, see TableSetupDrawChannels() for details.
// Unused channels don't perform their +2 allocations.
void ImGui::TableBeginInitMemory(ImGuiTable* table, int columns_count)
{
    // Allocate single buffer for our arrays
    ImSpanAllocator<3> span_allocator;
    span_allocator.Reserve(0, columns_count * sizeof(ImGuiTableColumn));
    span_allocator.Reserve(1, columns_count * sizeof(ImGuiTableColumnIdx));
    span_allocator.Reserve(2, columns_count * sizeof(ImGuiTableCellData), 4);
    table->RawData = IM_ALLOC(span_allocator.GetArenaSizeInBytes());
    memset(table->RawData, 0, span_allocator.GetArenaSizeInBytes());
    span_allocator.SetArenaBasePtr(table->RawData);
    span_allocator.GetSpan(0, &table->Columns);
    span_allocator.GetSpan(1, &table->DisplayOrderToIndex);
    span_allocator.GetSpan(2, &table->RowCellData);
}

// Apply queued resizing/reordering/hiding requests
void ImGui::TableBeginApplyRequests(ImGuiTable* table)
{
    // Handle resizing request
    // (We process this at the first TableBegin of the frame)
    // FIXME-TABLE: Contains columns if our work area doesn't allow for scrolling?
    if (table->InstanceCurrent == 0)
    {
        if (table->ResizedColumn != -1 && table->ResizedColumnNextWidth != FLT_MAX)
            TableSetColumnWidth(table->ResizedColumn, table->ResizedColumnNextWidth);
        table->LastResizedColumn = table->ResizedColumn;
        table->ResizedColumnNextWidth = FLT_MAX;
        table->ResizedColumn = -1;

        // Process auto-fit for single column, which is a special case for stretch columns and fixed columns with FixedSame policy.
        // FIXME-TABLE: Would be nice to redistribute available stretch space accordingly to other weights, instead of giving it all to siblings.
        if (table->AutoFitSingleColumn != -1)
        {
            TableSetColumnWidth(table->AutoFitSingleColumn, table->Columns[table->AutoFitSingleColumn].WidthAuto);
            table->AutoFitSingleColumn = -1;
        }
    }

    // Handle reordering request
    // Note: we don't clear ReorderColumn after handling the request.
    if (table->InstanceCurrent == 0)
    {
        if (table->HeldHeaderColumn == -1 && table->ReorderColumn != -1)
            table->ReorderColumn = -1;
        table->HeldHeaderColumn = -1;
        if (table->ReorderColumn != -1 && table->ReorderColumnDir != 0)
        {
            // We need to handle reordering across hidden columns.
            // In the configuration below, moving C to the right of E will lead to:
            //    ... C [D] E  --->  ... [D] E  C   (Column name/index)
            //    ... 2  3  4        ...  2  3  4   (Display order)
            const int reorder_dir = table->ReorderColumnDir;
            IM_ASSERT(reorder_dir == -1 || reorder_dir == +1);
            IM_ASSERT(table->Flags & ImGuiTableFlags_Reorderable);
            ImGuiTableColumn* src_column = &table->Columns[table->ReorderColumn];
            ImGuiTableColumn* dst_column = &table->Columns[(reorder_dir == -1) ? src_column->PrevEnabledColumn : src_column->NextEnabledColumn];
            IM_UNUSED(dst_column);
            const int src_order = src_column->DisplayOrder;
            const int dst_order = dst_column->DisplayOrder;
            src_column->DisplayOrder = (ImGuiTableColumnIdx)dst_order;
            for (int order_n = src_order + reorder_dir; order_n != dst_order + reorder_dir; order_n += reorder_dir)
                table->Columns[table->DisplayOrderToIndex[order_n]].DisplayOrder -= (ImGuiTableColumnIdx)reorder_dir;
            IM_ASSERT(dst_column->DisplayOrder == dst_order - reorder_dir);

            // Display order is stored in both columns->IndexDisplayOrder and table->DisplayOrder[],
            // rebuild the later from the former.
            for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
                table->DisplayOrderToIndex[table->Columns[column_n].DisplayOrder] = (ImGuiTableColumnIdx)column_n;
            table->ReorderColumnDir = 0;
            table->IsSettingsDirty = true;
        }
    }

    // Handle display order reset request
    if (table->IsResetDisplayOrderRequest)
    {
        for (int n = 0; n < table->ColumnsCount; n++)
            table->DisplayOrderToIndex[n] = table->Columns[n].DisplayOrder = (ImGuiTableColumnIdx)n;
        table->IsResetDisplayOrderRequest = false;
        table->IsSettingsDirty = true;
    }
}

// Adjust flags: default width mode + stretch columns are not allowed when auto extending
static void TableSetupColumnFlags(ImGuiTable* table, ImGuiTableColumn* column, ImGuiTableColumnFlags flags_in)
{
    ImGuiTableColumnFlags flags = flags_in;

    // Sizing Policy
    if ((flags & ImGuiTableColumnFlags_WidthMask_) == 0)
    {
        const ImGuiTableFlags table_sizing_policy = (table->Flags & ImGuiTableFlags_SizingMask_);
        if (table_sizing_policy == ImGuiTableFlags_SizingFixedFit || table_sizing_policy == ImGuiTableFlags_SizingFixedSame)
            flags |= ImGuiTableColumnFlags_WidthFixed;
        else
            flags |= ImGuiTableColumnFlags_WidthStretch;
    }
    else
    {
        IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiTableColumnFlags_WidthMask_)); // Check that only 1 of each set is used.
    }

    // Resize
    if ((table->Flags & ImGuiTableFlags_Resizable) == 0)
        flags |= ImGuiTableColumnFlags_NoResize;

    // Sorting
    if ((flags & ImGuiTableColumnFlags_NoSortAscending) && (flags & ImGuiTableColumnFlags_NoSortDescending))
        flags |= ImGuiTableColumnFlags_NoSort;

    // Indentation
    if ((flags & ImGuiTableColumnFlags_IndentMask_) == 0)
        flags |= (table->Columns.index_from_ptr(column) == 0) ? ImGuiTableColumnFlags_IndentEnable : ImGuiTableColumnFlags_IndentDisable;

    // Alignment
    //if ((flags & ImGuiTableColumnFlags_AlignMask_) == 0)
    //    flags |= ImGuiTableColumnFlags_AlignCenter;
    //IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiTableColumnFlags_AlignMask_)); // Check that only 1 of each set is used.

    // Preserve status flags
    column->Flags = flags | (column->Flags & ImGuiTableColumnFlags_StatusMask_);

    // Build an ordered list of available sort directions
    column->SortDirectionsAvailCount = column->SortDirectionsAvailMask = column->SortDirectionsAvailList = 0;
    if (table->Flags & ImGuiTableFlags_Sortable)
    {
        int count = 0, mask = 0, list = 0;
        if ((flags & ImGuiTableColumnFlags_PreferSortAscending) != 0 && (flags & ImGuiTableColumnFlags_NoSortAscending) == 0) { mask |= 1 << ImGuiSortDirection_Ascending;  list |= ImGuiSortDirection_Ascending << (count << 1); count++; }
        if ((flags & ImGuiTableColumnFlags_PreferSortDescending) != 0 && (flags & ImGuiTableColumnFlags_NoSortDescending) == 0) { mask |= 1 << ImGuiSortDirection_Descending; list |= ImGuiSortDirection_Descending << (count << 1); count++; }
        if ((flags & ImGuiTableColumnFlags_PreferSortAscending) == 0 && (flags & ImGuiTableColumnFlags_NoSortAscending) == 0) { mask |= 1 << ImGuiSortDirection_Ascending;  list |= ImGuiSortDirection_Ascending << (count << 1); count++; }
        if ((flags & ImGuiTableColumnFlags_PreferSortDescending) == 0 && (flags & ImGuiTableColumnFlags_NoSortDescending) == 0) { mask |= 1 << ImGuiSortDirection_Descending; list |= ImGuiSortDirection_Descending << (count << 1); count++; }
        if ((table->Flags & ImGuiTableFlags_SortTristate) || count == 0) { mask |= 1 << ImGuiSortDirection_None; count++; }
        column->SortDirectionsAvailList = (ImU8)list;
        column->SortDirectionsAvailMask = (ImU8)mask;
        column->SortDirectionsAvailCount = (ImU8)count;
        ImGui::TableFixColumnSortDirection(table, column);
    }
}

// Layout columns for the frame. This is in essence the followup to BeginTable().
// Runs on the first call to TableNextRow(), to give a chance for TableSetupColumn() to be called first.
// FIXME-TABLE: Our width (and therefore our WorkRect) will be minimal in the first frame for _WidthAuto columns.
// Increase feedback side-effect with widgets relying on WorkRect.Max.x... Maybe provide a default distribution for _WidthAuto columns?
void ImGui::TableUpdateLayout(ImGuiTable* table)
{
    ImGuiContext& g = *GImGui;
    IM_ASSERT(table->IsLayoutLocked == false);

    const ImGuiTableFlags table_sizing_policy = (table->Flags & ImGuiTableFlags_SizingMask_);
    table->IsDefaultDisplayOrder = true;
    table->ColumnsEnabledCount = 0;
    table->EnabledMaskByIndex = 0x00;
    table->EnabledMaskByDisplayOrder = 0x00;
    table->LeftMostEnabledColumn = -1;
    table->MinColumnWidth = ImMax(1.0f, g.Style.FramePadding.x * 1.0f); // g.Style.ColumnsMinSpacing; // FIXME-TABLE

    // [Part 1] Apply/lock Enabled and Order states. Calculate auto/ideal width for columns. Count fixed/stretch columns.
    // Process columns in their visible orders as we are building the Prev/Next indices.
    int count_fixed = 0;                // Number of columns that have fixed sizing policies
    int count_stretch = 0;              // Number of columns that have stretch sizing policies
    int prev_visible_column_idx = -1;
    bool has_auto_fit_request = false;
    bool has_resizable = false;
    float stretch_sum_width_auto = 0.0f;
    float fixed_max_width_auto = 0.0f;
    for (int order_n = 0; order_n < table->ColumnsCount; order_n++)
    {
        const int column_n = table->DisplayOrderToIndex[order_n];
        if (column_n != order_n)
            table->IsDefaultDisplayOrder = false;
        ImGuiTableColumn* column = &table->Columns[column_n];

        // Clear column setup if not submitted by user. Currently we make it mandatory to call TableSetupColumn() every frame.
        // It would easily work without but we're not ready to guarantee it since e.g. names need resubmission anyway.
        // We take a slight shortcut but in theory we could be calling TableSetupColumn() here with dummy values, it should yield the same effect.
        if (table->DeclColumnsCount <= column_n)
        {
            TableSetupColumnFlags(table, column, ImGuiTableColumnFlags_None);
            column->NameOffset = -1;
            column->UserID = 0;
            column->InitStretchWeightOrWidth = -1.0f;
        }

        // Update Enabled state, mark settings and sort specs dirty
        if (!(table->Flags & ImGuiTableFlags_Hideable) || (column->Flags & ImGuiTableColumnFlags_NoHide))
            column->IsUserEnabledNextFrame = true;
        if (column->IsUserEnabled != column->IsUserEnabledNextFrame)
        {
            column->IsUserEnabled = column->IsUserEnabledNextFrame;
            table->IsSettingsDirty = true;
        }
        column->IsEnabled = column->IsUserEnabled && (column->Flags & ImGuiTableColumnFlags_Disabled) == 0;

        if (column->SortOrder != -1 && !column->IsEnabled)
            table->IsSortSpecsDirty = true;
        if (column->SortOrder > 0 && !(table->Flags & ImGuiTableFlags_SortMulti))
            table->IsSortSpecsDirty = true;

        // Auto-fit unsized columns
        const bool start_auto_fit = (column->Flags & ImGuiTableColumnFlags_WidthFixed) ? (column->WidthRequest < 0.0f) : (column->StretchWeight < 0.0f);
        if (start_auto_fit)
            column->AutoFitQueue = column->CannotSkipItemsQueue = (1 << 3) - 1; // Fit for three frames

        if (!column->IsEnabled)
        {
            column->IndexWithinEnabledSet = -1;
            continue;
        }

        // Mark as enabled and link to previous/next enabled column
        column->PrevEnabledColumn = (ImGuiTableColumnIdx)prev_visible_column_idx;
        column->NextEnabledColumn = -1;
        if (prev_visible_column_idx != -1)
            table->Columns[prev_visible_column_idx].NextEnabledColumn = (ImGuiTableColumnIdx)column_n;
        else
            table->LeftMostEnabledColumn = (ImGuiTableColumnIdx)column_n;
        column->IndexWithinEnabledSet = table->ColumnsEnabledCount++;
        table->EnabledMaskByIndex |= (ImU64)1 << column_n;
        table->EnabledMaskByDisplayOrder |= (ImU64)1 << column->DisplayOrder;
        prev_visible_column_idx = column_n;
        IM_ASSERT(column->IndexWithinEnabledSet <= column->DisplayOrder);

        // Calculate ideal/auto column width (that's the width required for all contents to be visible without clipping)
        // Combine width from regular rows + width from headers unless requested not to.
        if (!column->IsPreserveWidthAuto)
            column->WidthAuto = TableGetColumnWidthAuto(table, column);

        // Non-resizable columns keep their requested width (apply user value regardless of IsPreserveWidthAuto)
        const bool column_is_resizable = (column->Flags & ImGuiTableColumnFlags_NoResize) == 0;
        if (column_is_resizable)
            has_resizable = true;
        if ((column->Flags & ImGuiTableColumnFlags_WidthFixed) && column->InitStretchWeightOrWidth > 0.0f && !column_is_resizable)
            column->WidthAuto = column->InitStretchWeightOrWidth;

        if (column->AutoFitQueue != 0x00)
            has_auto_fit_request = true;
        if (column->Flags & ImGuiTableColumnFlags_WidthStretch)
        {
            stretch_sum_width_auto += column->WidthAuto;
            count_stretch++;
        }
        else
        {
            fixed_max_width_auto = ImMax(fixed_max_width_auto, column->WidthAuto);
            count_fixed++;
        }
    }
    if ((table->Flags & ImGuiTableFlags_Sortable) && table->SortSpecsCount == 0 && !(table->Flags & ImGuiTableFlags_SortTristate))
        table->IsSortSpecsDirty = true;
    table->RightMostEnabledColumn = (ImGuiTableColumnIdx)prev_visible_column_idx;
    IM_ASSERT(table->LeftMostEnabledColumn >= 0 && table->RightMostEnabledColumn >= 0);

    // [Part 2] Disable child window clipping while fitting columns. This is not strictly necessary but makes it possible
    // to avoid the column fitting having to wait until the first visible frame of the child container (may or not be a good thing).
    // FIXME-TABLE: for always auto-resizing columns may not want to do that all the time.
    if (has_auto_fit_request && table->OuterWindow != table->InnerWindow)
        table->InnerWindow->SkipItems = false;
    if (has_auto_fit_request)
        table->IsSettingsDirty = true;

    // [Part 3] Fix column flags and record a few extra information.
    float sum_width_requests = 0.0f;        // Sum of all width for fixed and auto-resize columns, excluding width contributed by Stretch columns but including spacing/padding.
    float stretch_sum_weights = 0.0f;       // Sum of all weights for stretch columns.
    table->LeftMostStretchedColumn = table->RightMostStretchedColumn = -1;
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        if (!(table->EnabledMaskByIndex & ((ImU64)1 << column_n)))
            continue;
        ImGuiTableColumn* column = &table->Columns[column_n];

        const bool column_is_resizable = (column->Flags & ImGuiTableColumnFlags_NoResize) == 0;
        if (column->Flags & ImGuiTableColumnFlags_WidthFixed)
        {
            // Apply same widths policy
            float width_auto = column->WidthAuto;
            if (table_sizing_policy == ImGuiTableFlags_SizingFixedSame && (column->AutoFitQueue != 0x00 || !column_is_resizable))
                width_auto = fixed_max_width_auto;

            // Apply automatic width
            // Latch initial size for fixed columns and update it constantly for auto-resizing column (unless clipped!)
            if (column->AutoFitQueue != 0x00)
                column->WidthRequest = width_auto;
            else if ((column->Flags & ImGuiTableColumnFlags_WidthFixed) && !column_is_resizable && (table->RequestOutputMaskByIndex & ((ImU64)1 << column_n)))
                column->WidthRequest = width_auto;

            // FIXME-TABLE: Increase minimum size during init frame to avoid biasing auto-fitting widgets
            // (e.g. TextWrapped) too much. Otherwise what tends to happen is that TextWrapped would output a very
            // large height (= first frame scrollbar display very off + clipper would skip lots of items).
            // This is merely making the side-effect less extreme, but doesn't properly fixes it.
            // FIXME: Move this to ->WidthGiven to avoid temporary lossyless?
            // FIXME: This break IsPreserveWidthAuto from not flickering if the stored WidthAuto was smaller.
            if (column->AutoFitQueue > 0x01 && table->IsInitializing && !column->IsPreserveWidthAuto)
                column->WidthRequest = ImMax(column->WidthRequest, table->MinColumnWidth * 4.0f); // FIXME-TABLE: Another constant/scale?
            sum_width_requests += column->WidthRequest;
        }
        else
        {
            // Initialize stretch weight
            if (column->AutoFitQueue != 0x00 || column->StretchWeight < 0.0f || !column_is_resizable)
            {
                if (column->InitStretchWeightOrWidth > 0.0f)
                    column->StretchWeight = column->InitStretchWeightOrWidth;
                else if (table_sizing_policy == ImGuiTableFlags_SizingStretchProp)
                    column->StretchWeight = (column->WidthAuto / stretch_sum_width_auto) * count_stretch;
                else
                    column->StretchWeight = 1.0f;
            }

            stretch_sum_weights += column->StretchWeight;
            if (table->LeftMostStretchedColumn == -1 || table->Columns[table->LeftMostStretchedColumn].DisplayOrder > column->DisplayOrder)
                table->LeftMostStretchedColumn = (ImGuiTableColumnIdx)column_n;
            if (table->RightMostStretchedColumn == -1 || table->Columns[table->RightMostStretchedColumn].DisplayOrder < column->DisplayOrder)
                table->RightMostStretchedColumn = (ImGuiTableColumnIdx)column_n;
        }
        column->IsPreserveWidthAuto = false;
        sum_width_requests += table->CellPaddingX * 2.0f;
    }
    table->ColumnsEnabledFixedCount = (ImGuiTableColumnIdx)count_fixed;

    // [Part 4] Apply final widths based on requested widths
    const ImRect work_rect = table->WorkRect;
    const float width_spacings = (table->OuterPaddingX * 2.0f) + (table->CellSpacingX1 + table->CellSpacingX2) * (table->ColumnsEnabledCount - 1);
    const float width_avail = ((table->Flags & ImGuiTableFlags_ScrollX) && table->InnerWidth == 0.0f) ? table->InnerClipRect.GetWidth() : work_rect.GetWidth();
    const float width_avail_for_stretched_columns = width_avail - width_spacings - sum_width_requests;
    float width_remaining_for_stretched_columns = width_avail_for_stretched_columns;
    table->ColumnsGivenWidth = width_spacings + (table->CellPaddingX * 2.0f) * table->ColumnsEnabledCount;
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        if (!(table->EnabledMaskByIndex & ((ImU64)1 << column_n)))
            continue;
        ImGuiTableColumn* column = &table->Columns[column_n];

        // Allocate width for stretched/weighted columns (StretchWeight gets converted into WidthRequest)
        if (column->Flags & ImGuiTableColumnFlags_WidthStretch)
        {
            float weight_ratio = column->StretchWeight / stretch_sum_weights;
            column->WidthRequest = IM_FLOOR(ImMax(width_avail_for_stretched_columns * weight_ratio, table->MinColumnWidth) + 0.01f);
            width_remaining_for_stretched_columns -= column->WidthRequest;
        }

        // [Resize Rule 1] The right-most Visible column is not resizable if there is at least one Stretch column
        // See additional comments in TableSetColumnWidth().
        if (column->NextEnabledColumn == -1 && table->LeftMostStretchedColumn != -1)
            column->Flags |= ImGuiTableColumnFlags_NoDirectResize_;

        // Assign final width, record width in case we will need to shrink
        column->WidthGiven = ImFloor(ImMax(column->WidthRequest, table->MinColumnWidth));
        table->ColumnsGivenWidth += column->WidthGiven;
    }

    // [Part 5] Redistribute stretch remainder width due to rounding (remainder width is < 1.0f * number of Stretch column).
    // Using right-to-left distribution (more likely to match resizing cursor).
    if (width_remaining_for_stretched_columns >= 1.0f && !(table->Flags & ImGuiTableFlags_PreciseWidths))
        for (int order_n = table->ColumnsCount - 1; stretch_sum_weights > 0.0f && width_remaining_for_stretched_columns >= 1.0f && order_n >= 0; order_n--)
        {
            if (!(table->EnabledMaskByDisplayOrder & ((ImU64)1 << order_n)))
                continue;
            ImGuiTableColumn* column = &table->Columns[table->DisplayOrderToIndex[order_n]];
            if (!(column->Flags & ImGuiTableColumnFlags_WidthStretch))
                continue;
            column->WidthRequest += 1.0f;
            column->WidthGiven += 1.0f;
            width_remaining_for_stretched_columns -= 1.0f;
        }

    ImGuiTableInstanceData* table_instance = TableGetInstanceData(table, table->InstanceCurrent);
    table->HoveredColumnBody = -1;
    table->HoveredColumnBorder = -1;
    const ImRect mouse_hit_rect(table->OuterRect.Min.x, table->OuterRect.Min.y, table->OuterRect.Max.x, ImMax(table->OuterRect.Max.y, table->OuterRect.Min.y + table_instance->LastOuterHeight));
    const bool is_hovering_table = ItemHoverable(mouse_hit_rect, 0);

    // [Part 6] Setup final position, offset, skip/clip states and clipping rectangles, detect hovered column
    // Process columns in their visible orders as we are comparing the visible order and adjusting host_clip_rect while looping.
    int visible_n = 0;
    bool offset_x_frozen = (table->FreezeColumnsCount > 0);
    float offset_x = ((table->FreezeColumnsCount > 0) ? table->OuterRect.Min.x : work_rect.Min.x) + table->OuterPaddingX - table->CellSpacingX1;
    ImRect host_clip_rect = table->InnerClipRect;
    //host_clip_rect.Max.x += table->CellPaddingX + table->CellSpacingX2;
    table->VisibleMaskByIndex = 0x00;
    table->RequestOutputMaskByIndex = 0x00;
    for (int order_n = 0; order_n < table->ColumnsCount; order_n++)
    {
        const int column_n = table->DisplayOrderToIndex[order_n];
        ImGuiTableColumn* column = &table->Columns[column_n];

        column->NavLayerCurrent = (ImS8)((table->FreezeRowsCount > 0 || column_n < table->FreezeColumnsCount) ? ImGuiNavLayer_Menu : ImGuiNavLayer_Main);

        if (offset_x_frozen && table->FreezeColumnsCount == visible_n)
        {
            offset_x += work_rect.Min.x - table->OuterRect.Min.x;
            offset_x_frozen = false;
        }

        // Clear status flags
        column->Flags &= ~ImGuiTableColumnFlags_StatusMask_;

        if ((table->EnabledMaskByDisplayOrder & ((ImU64)1 << order_n)) == 0)
        {
            // Hidden column: clear a few fields and we are done with it for the remainder of the function.
            // We set a zero-width clip rect but set Min.y/Max.y properly to not interfere with the clipper.
            column->MinX = column->MaxX = column->WorkMinX = column->ClipRect.Min.x = column->ClipRect.Max.x = offset_x;
            column->WidthGiven = 0.0f;
            column->ClipRect.Min.y = work_rect.Min.y;
            column->ClipRect.Max.y = FLT_MAX;
            column->ClipRect.ClipWithFull(host_clip_rect);
            column->IsVisibleX = column->IsVisibleY = column->IsRequestOutput = false;
            column->IsSkipItems = true;
            column->ItemWidth = 1.0f;
            continue;
        }

        // Detect hovered column
        if (is_hovering_table && g.IO.MousePos.x >= column->ClipRect.Min.x && g.IO.MousePos.x < column->ClipRect.Max.x)
            table->HoveredColumnBody = (ImGuiTableColumnIdx)column_n;

        // Lock start position
        column->MinX = offset_x;

        // Lock width based on start position and minimum/maximum width for this position
        float max_width = TableGetMaxColumnWidth(table, column_n);
        column->WidthGiven = ImMin(column->WidthGiven, max_width);
        column->WidthGiven = ImMax(column->WidthGiven, ImMin(column->WidthRequest, table->MinColumnWidth));
        column->MaxX = offset_x + column->WidthGiven + table->CellSpacingX1 + table->CellSpacingX2 + table->CellPaddingX * 2.0f;

        // Lock other positions
        // - ClipRect.Min.x: Because merging draw commands doesn't compare min boundaries, we make ClipRect.Min.x match left bounds to be consistent regardless of merging.
        // - ClipRect.Max.x: using WorkMaxX instead of MaxX (aka including padding) makes things more consistent when resizing down, tho slightly detrimental to visibility in very-small column.
        // - ClipRect.Max.x: using MaxX makes it easier for header to receive hover highlight with no discontinuity and display sorting arrow.
        // - FIXME-TABLE: We want equal width columns to have equal (ClipRect.Max.x - WorkMinX) width, which means ClipRect.max.x cannot stray off host_clip_rect.Max.x else right-most column may appear shorter.
        column->WorkMinX = column->MinX + table->CellPaddingX + table->CellSpacingX1;
        column->WorkMaxX = column->MaxX - table->CellPaddingX - table->CellSpacingX2; // Expected max
        column->ItemWidth = ImFloor(column->WidthGiven * 0.65f);
        column->ClipRect.Min.x = column->MinX;
        column->ClipRect.Min.y = work_rect.Min.y;
        column->ClipRect.Max.x = column->MaxX; //column->WorkMaxX;
        column->ClipRect.Max.y = FLT_MAX;
        column->ClipRect.ClipWithFull(host_clip_rect);

        // Mark column as Clipped (not in sight)
        // Note that scrolling tables (where inner_window != outer_window) handle Y clipped earlier in BeginTable() so IsVisibleY really only applies to non-scrolling tables.
        // FIXME-TABLE: Because InnerClipRect.Max.y is conservatively ==outer_window->ClipRect.Max.y, we never can mark columns _Above_ the scroll line as not IsVisibleY.
        // Taking advantage of LastOuterHeight would yield good results there...
        // FIXME-TABLE: Y clipping is disabled because it effectively means not submitting will reduce contents width which is fed to outer_window->DC.CursorMaxPos.x,
        // and this may be used (e.g. typically by outer_window using AlwaysAutoResize or outer_window's horizontal scrollbar, but could be something else).
        // Possible solution to preserve last known content width for clipped column. Test 'table_reported_size' fails when enabling Y clipping and window is resized small.
        column->IsVisibleX = (column->ClipRect.Max.x > column->ClipRect.Min.x);
        column->IsVisibleY = true; // (column->ClipRect.Max.y > column->ClipRect.Min.y);
        const bool is_visible = column->IsVisibleX; //&& column->IsVisibleY;
        if (is_visible)
            table->VisibleMaskByIndex |= ((ImU64)1 << column_n);

        // Mark column as requesting output from user. Note that fixed + non-resizable sets are auto-fitting at all times and therefore always request output.
        column->IsRequestOutput = is_visible || column->AutoFitQueue != 0 || column->CannotSkipItemsQueue != 0;
        if (column->IsRequestOutput)
            table->RequestOutputMaskByIndex |= ((ImU64)1 << column_n);

        // Mark column as SkipItems (ignoring all items/layout)
        column->IsSkipItems = !column->IsEnabled || table->HostSkipItems;
        if (column->IsSkipItems)
            IM_ASSERT(!is_visible);

        // Update status flags
        column->Flags |= ImGuiTableColumnFlags_IsEnabled;
        if (is_visible)
            column->Flags |= ImGuiTableColumnFlags_IsVisible;
        if (column->SortOrder != -1)
            column->Flags |= ImGuiTableColumnFlags_IsSorted;
        if (table->HoveredColumnBody == column_n)
            column->Flags |= ImGuiTableColumnFlags_IsHovered;

        // Alignment
        // FIXME-TABLE: This align based on the whole column width, not per-cell, and therefore isn't useful in
        // many cases (to be able to honor this we might be able to store a log of cells width, per row, for
        // visible rows, but nav/programmatic scroll would have visible artifacts.)
        //if (column->Flags & ImGuiTableColumnFlags_AlignRight)
        //    column->WorkMinX = ImMax(column->WorkMinX, column->MaxX - column->ContentWidthRowsUnfrozen);
        //else if (column->Flags & ImGuiTableColumnFlags_AlignCenter)
        //    column->WorkMinX = ImLerp(column->WorkMinX, ImMax(column->StartX, column->MaxX - column->ContentWidthRowsUnfrozen), 0.5f);

        // Reset content width variables
        column->ContentMaxXFrozen = column->ContentMaxXUnfrozen = column->WorkMinX;
        column->ContentMaxXHeadersUsed = column->ContentMaxXHeadersIdeal = column->WorkMinX;

        // Don't decrement auto-fit counters until container window got a chance to submit its items
        if (table->HostSkipItems == false)
        {
            column->AutoFitQueue >>= 1;
            column->CannotSkipItemsQueue >>= 1;
        }

        if (visible_n < table->FreezeColumnsCount)
            host_clip_rect.Min.x = ImClamp(column->MaxX + TABLE_BORDER_SIZE, host_clip_rect.Min.x, host_clip_rect.Max.x);

        offset_x += column->WidthGiven + table->CellSpacingX1 + table->CellSpacingX2 + table->CellPaddingX * 2.0f;
        visible_n++;
    }

    // [Part 7] Detect/store when we are hovering the unused space after the right-most column (so e.g. context menus can react on it)
    // Clear Resizable flag if none of our column are actually resizable (either via an explicit _NoResize flag, either
    // because of using _WidthAuto/_WidthStretch). This will hide the resizing option from the context menu.
    const float unused_x1 = ImMax(table->WorkRect.Min.x, table->Columns[table->RightMostEnabledColumn].ClipRect.Max.x);
    if (is_hovering_table && table->HoveredColumnBody == -1)
    {
        if (g.IO.MousePos.x >= unused_x1)
            table->HoveredColumnBody = (ImGuiTableColumnIdx)table->ColumnsCount;
    }
    if (has_resizable == false && (table->Flags & ImGuiTableFlags_Resizable))
        table->Flags &= ~ImGuiTableFlags_Resizable;

    // [Part 8] Lock actual OuterRect/WorkRect right-most position.
    // This is done late to handle the case of fixed-columns tables not claiming more widths that they need.
    // Because of this we are careful with uses of WorkRect and InnerClipRect before this point.
    if (table->RightMostStretchedColumn != -1)
        table->Flags &= ~ImGuiTableFlags_NoHostExtendX;
    if (table->Flags & ImGuiTableFlags_NoHostExtendX)
    {
        table->OuterRect.Max.x = table->WorkRect.Max.x = unused_x1;
        table->InnerClipRect.Max.x = ImMin(table->InnerClipRect.Max.x, unused_x1);
    }
    table->InnerWindow->ParentWorkRect = table->WorkRect;
    table->BorderX1 = table->InnerClipRect.Min.x;// +((table->Flags & ImGuiTableFlags_BordersOuter) ? 0.0f : -1.0f);
    table->BorderX2 = table->InnerClipRect.Max.x;// +((table->Flags & ImGuiTableFlags_BordersOuter) ? 0.0f : +1.0f);

    // [Part 9] Allocate draw channels and setup background cliprect
    TableSetupDrawChannels(table);

    // [Part 10] Hit testing on borders
    if (table->Flags & ImGuiTableFlags_Resizable)
        TableUpdateBorders(table);
    table_instance->LastFirstRowHeight = 0.0f;
    table->IsLayoutLocked = true;
    table->IsUsingHeaders = false;

    // [Part 11] Context menu
    if (table->IsContextPopupOpen && table->InstanceCurrent == table->InstanceInteracted)
    {
        const ImGuiID context_menu_id = ImHashStr("##ContextMenu", 0, table->ID);
        if (BeginPopupEx(context_menu_id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
        {
            TableDrawContextMenu(table);
            EndPopup();
        }
        else
        {
            table->IsContextPopupOpen = false;
        }
    }

    // [Part 13] Sanitize and build sort specs before we have a change to use them for display.
    // This path will only be exercised when sort specs are modified before header rows (e.g. init or visibility change)
    if (table->IsSortSpecsDirty && (table->Flags & ImGuiTableFlags_Sortable))
        TableSortSpecsBuild(table);

    // Initial state
    ImGuiWindow* inner_window = table->InnerWindow;
    if (table->Flags & ImGuiTableFlags_NoClip)
        table->DrawSplitter->SetCurrentChannel(inner_window->DrawList, TABLE_DRAW_CHANNEL_NOCLIP);
    else
        inner_window->DrawList->PushClipRect(inner_window->ClipRect.Min, inner_window->ClipRect.Max, false);
}

// Process hit-testing on resizing borders. Actual size change will be applied in EndTable()
// - Set table->HoveredColumnBorder with a short delay/timer to reduce feedback noise
// - Submit ahead of table contents and header, use ImGuiButtonFlags_AllowItemOverlap to prioritize widgets
//   overlapping the same area.
void ImGui::TableUpdateBorders(ImGuiTable* table)
{
    ImGuiContext& g = *GImGui;
    IM_ASSERT(table->Flags & ImGuiTableFlags_Resizable);

    // At this point OuterRect height may be zero or under actual final height, so we rely on temporal coherency and
    // use the final height from last frame. Because this is only affecting _interaction_ with columns, it is not
    // really problematic (whereas the actual visual will be displayed in EndTable() and using the current frame height).
    // Actual columns highlight/render will be performed in EndTable() and not be affected.
    ImGuiTableInstanceData* table_instance = TableGetInstanceData(table, table->InstanceCurrent);
    const float hit_half_width = TABLE_RESIZE_SEPARATOR_HALF_THICKNESS;
    const float hit_y1 = table->OuterRect.Min.y;
    const float hit_y2_body = ImMax(table->OuterRect.Max.y, hit_y1 + table_instance->LastOuterHeight);
    const float hit_y2_head = hit_y1 + table_instance->LastFirstRowHeight;

    for (int order_n = 0; order_n < table->ColumnsCount; order_n++)
    {
        if (!(table->EnabledMaskByDisplayOrder & ((ImU64)1 << order_n)))
            continue;

        const int column_n = table->DisplayOrderToIndex[order_n];
        ImGuiTableColumn* column = &table->Columns[column_n];
        if (column->Flags & (ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoDirectResize_))
            continue;

        // ImGuiTableFlags_NoBordersInBodyUntilResize will be honored in TableDrawBorders()
        const float border_y2_hit = (table->Flags & ImGuiTableFlags_NoBordersInBody) ? hit_y2_head : hit_y2_body;
        if ((table->Flags & ImGuiTableFlags_NoBordersInBody) && table->IsUsingHeaders == false)
            continue;

        if (!column->IsVisibleX && table->LastResizedColumn != column_n)
            continue;

        ImGuiID column_id = TableGetColumnResizeID(table, column_n, table->InstanceCurrent);
        ImRect hit_rect(column->MaxX - hit_half_width, hit_y1, column->MaxX + hit_half_width, border_y2_hit);
        //GetForegroundDrawList()->AddRect(hit_rect.Min, hit_rect.Max, IM_COL32(255, 0, 0, 100));
        KeepAliveID(column_id);

        bool hovered = false, held = false;
        bool pressed = ButtonBehavior(hit_rect, column_id, &hovered, &held, ImGuiButtonFlags_FlattenChildren | ImGuiButtonFlags_AllowItemOverlap | ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_PressedOnDoubleClick | ImGuiButtonFlags_NoNavFocus);
        if (pressed && IsMouseDoubleClicked(0))
        {
            TableSetColumnWidthAutoSingle(table, column_n);
            ClearActiveID();
            held = hovered = false;
        }
        if (held)
        {
            if (table->LastResizedColumn == -1)
                table->ResizeLockMinContentsX2 = table->RightMostEnabledColumn != -1 ? table->Columns[table->RightMostEnabledColumn].MaxX : -FLT_MAX;
            table->ResizedColumn = (ImGuiTableColumnIdx)column_n;
            table->InstanceInteracted = table->InstanceCurrent;
        }
        if ((hovered && g.HoveredIdTimer > TABLE_RESIZE_SEPARATOR_FEEDBACK_TIMER) || held)
        {
            table->HoveredColumnBorder = (ImGuiTableColumnIdx)column_n;
            SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
    }
}

void    ImGui::EndTable()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && "Only call EndTable() if BeginTable() returns true!");

    // This assert would be very useful to catch a common error... unfortunately it would probably trigger in some
    // cases, and for consistency user may sometimes output empty tables (and still benefit from e.g. outer border)
    //IM_ASSERT(table->IsLayoutLocked && "Table unused: never called TableNextRow(), is that the intent?");

    // If the user never got to call TableNextRow() or TableNextColumn(), we call layout ourselves to ensure all our
    // code paths are consistent (instead of just hoping that TableBegin/TableEnd will work), get borders drawn, etc.
    if (!table->IsLayoutLocked)
        TableUpdateLayout(table);

    const ImGuiTableFlags flags = table->Flags;
    ImGuiWindow* inner_window = table->InnerWindow;
    ImGuiWindow* outer_window = table->OuterWindow;
    ImGuiTableTempData* temp_data = table->TempData;
    IM_ASSERT(inner_window == g.CurrentWindow);
    IM_ASSERT(outer_window == inner_window || outer_window == inner_window->ParentWindow);

    if (table->IsInsideRow)
        TableEndRow(table);

    // Context menu in columns body
    if (flags & ImGuiTableFlags_ContextMenuInBody)
        if (table->HoveredColumnBody != -1 && !IsAnyItemHovered() && IsMouseReleased(ImGuiMouseButton_Right))
            TableOpenContextMenu((int)table->HoveredColumnBody);

    // Finalize table height
    ImGuiTableInstanceData* table_instance = TableGetInstanceData(table, table->InstanceCurrent);
    inner_window->DC.PrevLineSize = temp_data->HostBackupPrevLineSize;
    inner_window->DC.CurrLineSize = temp_data->HostBackupCurrLineSize;
    inner_window->DC.CursorMaxPos = temp_data->HostBackupCursorMaxPos;
    const float inner_content_max_y = table->RowPosY2;
    IM_ASSERT(table->RowPosY2 == inner_window->DC.CursorPos.y);
    if (inner_window != outer_window)
        inner_window->DC.CursorMaxPos.y = inner_content_max_y;
    else if (!(flags & ImGuiTableFlags_NoHostExtendY))
        table->OuterRect.Max.y = table->InnerRect.Max.y = ImMax(table->OuterRect.Max.y, inner_content_max_y); // Patch OuterRect/InnerRect height
    table->WorkRect.Max.y = ImMax(table->WorkRect.Max.y, table->OuterRect.Max.y);
    table_instance->LastOuterHeight = table->OuterRect.GetHeight();

    // Setup inner scrolling range
    // FIXME: This ideally should be done earlier, in BeginTable() SetNextWindowContentSize call, just like writing to inner_window->DC.CursorMaxPos.y,
    // but since the later is likely to be impossible to do we'd rather update both axises together.
    if (table->Flags & ImGuiTableFlags_ScrollX)
    {
        const float outer_padding_for_border = (table->Flags & ImGuiTableFlags_BordersOuterV) ? TABLE_BORDER_SIZE : 0.0f;
        float max_pos_x = table->InnerWindow->DC.CursorMaxPos.x;
        if (table->RightMostEnabledColumn != -1)
            max_pos_x = ImMax(max_pos_x, table->Columns[table->RightMostEnabledColumn].WorkMaxX + table->CellPaddingX + table->OuterPaddingX - outer_padding_for_border);
        if (table->ResizedColumn != -1)
            max_pos_x = ImMax(max_pos_x, table->ResizeLockMinContentsX2);
        table->InnerWindow->DC.CursorMaxPos.x = max_pos_x;
    }

    // Pop clipping rect
    if (!(flags & ImGuiTableFlags_NoClip))
        inner_window->DrawList->PopClipRect();
    inner_window->ClipRect = inner_window->DrawList->_ClipRectStack.back();

    // Draw borders
    if ((flags & ImGuiTableFlags_Borders) != 0)
        TableDrawBorders(table);

#if 0
    // Strip out dummy channel draw calls
    // We have no way to prevent user submitting direct ImDrawList calls into a hidden column (but ImGui:: calls will be clipped out)
    // Pros: remove draw calls which will have no effect. since they'll have zero-size cliprect they may be early out anyway.
    // Cons: making it harder for users watching metrics/debugger to spot the wasted vertices.
    if (table->DummyDrawChannel != (ImGuiTableColumnIdx)-1)
    {
        ImDrawChannel* dummy_channel = &table->DrawSplitter._Channels[table->DummyDrawChannel];
        dummy_channel->_CmdBuffer.resize(0);
        dummy_channel->_IdxBuffer.resize(0);
    }
#endif

    // Flatten channels and merge draw calls
    ImDrawListSplitter* splitter = table->DrawSplitter;
    splitter->SetCurrentChannel(inner_window->DrawList, 0);
    if ((table->Flags & ImGuiTableFlags_NoClip) == 0)
        TableMergeDrawChannels(table);
    splitter->Merge(inner_window->DrawList);

    // Update ColumnsAutoFitWidth to get us ahead for host using our size to auto-resize without waiting for next BeginTable()
    const float width_spacings = (table->OuterPaddingX * 2.0f) + (table->CellSpacingX1 + table->CellSpacingX2) * (table->ColumnsEnabledCount - 1);
    table->ColumnsAutoFitWidth = width_spacings + (table->CellPaddingX * 2.0f) * table->ColumnsEnabledCount;
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
        if (table->EnabledMaskByIndex & ((ImU64)1 << column_n))
        {
            ImGuiTableColumn* column = &table->Columns[column_n];
            if ((column->Flags & ImGuiTableColumnFlags_WidthFixed) && !(column->Flags & ImGuiTableColumnFlags_NoResize))
                table->ColumnsAutoFitWidth += column->WidthRequest;
            else
                table->ColumnsAutoFitWidth += TableGetColumnWidthAuto(table, column);
        }

    // Update scroll
    if ((table->Flags & ImGuiTableFlags_ScrollX) == 0 && inner_window != outer_window)
    {
        inner_window->Scroll.x = 0.0f;
    }
    else if (table->LastResizedColumn != -1 && table->ResizedColumn == -1 && inner_window->ScrollbarX && table->InstanceInteracted == table->InstanceCurrent)
    {
        // When releasing a column being resized, scroll to keep the resulting column in sight
        const float neighbor_width_to_keep_visible = table->MinColumnWidth + table->CellPaddingX * 2.0f;
        ImGuiTableColumn* column = &table->Columns[table->LastResizedColumn];
        if (column->MaxX < table->InnerClipRect.Min.x)
            SetScrollFromPosX(inner_window, column->MaxX - inner_window->Pos.x - neighbor_width_to_keep_visible, 1.0f);
        else if (column->MaxX > table->InnerClipRect.Max.x)
            SetScrollFromPosX(inner_window, column->MaxX - inner_window->Pos.x + neighbor_width_to_keep_visible, 1.0f);
    }

    // Apply resizing/dragging at the end of the frame
    if (table->ResizedColumn != -1 && table->InstanceCurrent == table->InstanceInteracted)
    {
        ImGuiTableColumn* column = &table->Columns[table->ResizedColumn];
        const float new_x2 = (g.IO.MousePos.x - g.ActiveIdClickOffset.x + TABLE_RESIZE_SEPARATOR_HALF_THICKNESS);
        const float new_width = ImFloor(new_x2 - column->MinX - table->CellSpacingX1 - table->CellPaddingX * 2.0f);
        table->ResizedColumnNextWidth = new_width;
    }

    // Pop from id stack
    IM_ASSERT_USER_ERROR(inner_window->IDStack.back() == table->ID + table->InstanceCurrent, "Mismatching PushID/PopID!");
    IM_ASSERT_USER_ERROR(outer_window->DC.ItemWidthStack.Size >= temp_data->HostBackupItemWidthStackSize, "Too many PopItemWidth!");
    PopID();

    // Restore window data that we modified
    const ImVec2 backup_outer_max_pos = outer_window->DC.CursorMaxPos;
    inner_window->WorkRect = temp_data->HostBackupWorkRect;
    inner_window->ParentWorkRect = temp_data->HostBackupParentWorkRect;
    inner_window->SkipItems = table->HostSkipItems;
    outer_window->DC.CursorPos = table->OuterRect.Min;
    outer_window->DC.ItemWidth = temp_data->HostBackupItemWidth;
    outer_window->DC.ItemWidthStack.Size = temp_data->HostBackupItemWidthStackSize;
    outer_window->DC.ColumnsOffset = temp_data->HostBackupColumnsOffset;

    // Layout in outer window
    // (FIXME: To allow auto-fit and allow desirable effect of SameLine() we dissociate 'used' vs 'ideal' size by overriding
    // CursorPosPrevLine and CursorMaxPos manually. That should be a more general layout feature, see same problem e.g. #3414)
    if (inner_window != outer_window)
    {
        EndChild();
    }
    else
    {
        ItemSize(table->OuterRect.GetSize());
        ItemAdd(table->OuterRect, 0);
    }

    // Override declared contents width/height to enable auto-resize while not needlessly adding a scrollbar
    if (table->Flags & ImGuiTableFlags_NoHostExtendX)
    {
        // FIXME-TABLE: Could we remove this section?
        // ColumnsAutoFitWidth may be one frame ahead here since for Fixed+NoResize is calculated from latest contents
        IM_ASSERT((table->Flags & ImGuiTableFlags_ScrollX) == 0);
        outer_window->DC.CursorMaxPos.x = ImMax(backup_outer_max_pos.x, table->OuterRect.Min.x + table->ColumnsAutoFitWidth);
    }
    else if (temp_data->UserOuterSize.x <= 0.0f)
    {
        const float decoration_size = (table->Flags & ImGuiTableFlags_ScrollX) ? inner_window->ScrollbarSizes.x : 0.0f;
        outer_window->DC.IdealMaxPos.x = ImMax(outer_window->DC.IdealMaxPos.x, table->OuterRect.Min.x + table->ColumnsAutoFitWidth + decoration_size - temp_data->UserOuterSize.x);
        outer_window->DC.CursorMaxPos.x = ImMax(backup_outer_max_pos.x, ImMin(table->OuterRect.Max.x, table->OuterRect.Min.x + table->ColumnsAutoFitWidth));
    }
    else
    {
        outer_window->DC.CursorMaxPos.x = ImMax(backup_outer_max_pos.x, table->OuterRect.Max.x);
    }
    if (temp_data->UserOuterSize.y <= 0.0f)
    {
        const float decoration_size = (table->Flags & ImGuiTableFlags_ScrollY) ? inner_window->ScrollbarSizes.y : 0.0f;
        outer_window->DC.IdealMaxPos.y = ImMax(outer_window->DC.IdealMaxPos.y, inner_content_max_y + decoration_size - temp_data->UserOuterSize.y);
        outer_window->DC.CursorMaxPos.y = ImMax(backup_outer_max_pos.y, ImMin(table->OuterRect.Max.y, inner_content_max_y));
    }
    else
    {
        // OuterRect.Max.y may already have been pushed downward from the initial value (unless ImGuiTableFlags_NoHostExtendY is set)
        outer_window->DC.CursorMaxPos.y = ImMax(backup_outer_max_pos.y, table->OuterRect.Max.y);
    }

    // Save settings
    if (table->IsSettingsDirty)
        TableSaveSettings(table);
    table->IsInitializing = false;

    // Clear or restore current table, if any
    IM_ASSERT(g.CurrentWindow == outer_window && g.CurrentTable == table);
    IM_ASSERT(g.TablesTempDataStacked > 0);
    temp_data = (--g.TablesTempDataStacked > 0) ? &g.TablesTempData[g.TablesTempDataStacked - 1] : NULL;
    g.CurrentTable = temp_data ? g.Tables.GetByIndex(temp_data->TableIndex) : NULL;
    if (g.CurrentTable)
    {
        g.CurrentTable->TempData = temp_data;
        g.CurrentTable->DrawSplitter = &temp_data->DrawSplitter;
    }
    outer_window->DC.CurrentTableIdx = g.CurrentTable ? g.Tables.GetIndex(g.CurrentTable) : -1;
}

// See "COLUMN SIZING POLICIES" comments at the top of this file
// If (init_width_or_weight <= 0.0f) it is ignored
void ImGui::TableSetupColumn(const char* label, ImGuiTableColumnFlags flags, float init_width_or_weight, ImGuiID user_id)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && "Need to call TableSetupColumn() after BeginTable()!");
    IM_ASSERT(table->IsLayoutLocked == false && "Need to call call TableSetupColumn() before first row!");
    IM_ASSERT((flags & ImGuiTableColumnFlags_StatusMask_) == 0 && "Illegal to pass StatusMask values to TableSetupColumn()");
    if (table->DeclColumnsCount >= table->ColumnsCount)
    {
        IM_ASSERT_USER_ERROR(table->DeclColumnsCount < table->ColumnsCount, "Called TableSetupColumn() too many times!");
        return;
    }

    ImGuiTableColumn* column = &table->Columns[table->DeclColumnsCount];
    table->DeclColumnsCount++;

    // Assert when passing a width or weight if policy is entirely left to default, to avoid storing width into weight and vice-versa.
    // Give a grace to users of ImGuiTableFlags_ScrollX.
    if (table->IsDefaultSizingPolicy && (flags & ImGuiTableColumnFlags_WidthMask_) == 0 && (flags & ImGuiTableFlags_ScrollX) == 0)
        IM_ASSERT(init_width_or_weight <= 0.0f && "Can only specify width/weight if sizing policy is set explicitly in either Table or Column.");

    // When passing a width automatically enforce WidthFixed policy
    // (whereas TableSetupColumnFlags would default to WidthAuto if table is not Resizable)
    if ((flags & ImGuiTableColumnFlags_WidthMask_) == 0 && init_width_or_weight > 0.0f)
        if ((table->Flags & ImGuiTableFlags_SizingMask_) == ImGuiTableFlags_SizingFixedFit || (table->Flags & ImGuiTableFlags_SizingMask_) == ImGuiTableFlags_SizingFixedSame)
            flags |= ImGuiTableColumnFlags_WidthFixed;

    TableSetupColumnFlags(table, column, flags);
    column->UserID = user_id;
    flags = column->Flags;

    // Initialize defaults
    column->InitStretchWeightOrWidth = init_width_or_weight;
    if (table->IsInitializing)
    {
        // Init width or weight
        if (column->WidthRequest < 0.0f && column->StretchWeight < 0.0f)
        {
            if ((flags & ImGuiTableColumnFlags_WidthFixed) && init_width_or_weight > 0.0f)
                column->WidthRequest = init_width_or_weight;
            if (flags & ImGuiTableColumnFlags_WidthStretch)
                column->StretchWeight = (init_width_or_weight > 0.0f) ? init_width_or_weight : -1.0f;

            // Disable auto-fit if an explicit width/weight has been specified
            if (init_width_or_weight > 0.0f)
                column->AutoFitQueue = 0x00;
        }

        // Init default visibility/sort state
        if ((flags & ImGuiTableColumnFlags_DefaultHide) && (table->SettingsLoadedFlags & ImGuiTableFlags_Hideable) == 0)
            column->IsUserEnabled = column->IsUserEnabledNextFrame = false;
        if (flags & ImGuiTableColumnFlags_DefaultSort && (table->SettingsLoadedFlags & ImGuiTableFlags_Sortable) == 0)
        {
            column->SortOrder = 0; // Multiple columns using _DefaultSort will be reassigned unique SortOrder values when building the sort specs.
            column->SortDirection = (column->Flags & ImGuiTableColumnFlags_PreferSortDescending) ? (ImS8)ImGuiSortDirection_Descending : (ImU8)(ImGuiSortDirection_Ascending);
        }
    }

    // Store name (append with zero-terminator in contiguous buffer)
    column->NameOffset = -1;
    if (label != NULL && label[0] != 0)
    {
        column->NameOffset = (ImS16)table->ColumnsNames.size();
        table->ColumnsNames.append(label, label + strlen(label) + 1);
    }
}

// [Public]
void ImGui::TableSetupScrollFreeze(int columns, int rows)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && "Need to call TableSetupColumn() after BeginTable()!");
    IM_ASSERT(table->IsLayoutLocked == false && "Need to call TableSetupColumn() before first row!");
    IM_ASSERT(columns >= 0 && columns < IMGUI_TABLE_MAX_COLUMNS);
    IM_ASSERT(rows >= 0 && rows < 128); // Arbitrary limit

    table->FreezeColumnsRequest = (table->Flags & ImGuiTableFlags_ScrollX) ? (ImGuiTableColumnIdx)ImMin(columns, table->ColumnsCount) : 0;
    table->FreezeColumnsCount = (table->InnerWindow->Scroll.x != 0.0f) ? table->FreezeColumnsRequest : 0;
    table->FreezeRowsRequest = (table->Flags & ImGuiTableFlags_ScrollY) ? (ImGuiTableColumnIdx)rows : 0;
    table->FreezeRowsCount = (table->InnerWindow->Scroll.y != 0.0f) ? table->FreezeRowsRequest : 0;
    table->IsUnfrozenRows = (table->FreezeRowsCount == 0); // Make sure this is set before TableUpdateLayout() so ImGuiListClipper can benefit from it.b

    // Ensure frozen columns are ordered in their section. We still allow multiple frozen columns to be reordered.
    // FIXME-TABLE: This work for preserving 2143 into 21|43. How about 4321 turning into 21|43? (preserve relative order in each section)
    for (int column_n = 0; column_n < table->FreezeColumnsRequest; column_n++)
    {
        int order_n = table->DisplayOrderToIndex[column_n];
        if (order_n != column_n && order_n >= table->FreezeColumnsRequest)
        {
            ImSwap(table->Columns[table->DisplayOrderToIndex[order_n]].DisplayOrder, table->Columns[table->DisplayOrderToIndex[column_n]].DisplayOrder);
            ImSwap(table->DisplayOrderToIndex[order_n], table->DisplayOrderToIndex[column_n]);
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Tables: Simple accessors
//-----------------------------------------------------------------------------
// - TableGetColumnCount()
// - TableGetColumnName()
// - TableGetColumnName() [Internal]
// - TableSetColumnEnabled()
// - TableGetColumnFlags()
// - TableGetCellBgRect() [Internal]
// - TableGetColumnResizeID() [Internal]
// - TableGetHoveredColumn() [Internal]
// - TableSetBgColor()
//-----------------------------------------------------------------------------

int ImGui::TableGetColumnCount()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    return table ? table->ColumnsCount : 0;
}

const char* ImGui::TableGetColumnName(int column_n)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return NULL;
    if (column_n < 0)
        column_n = table->CurrentColumn;
    return TableGetColumnName(table, column_n);
}

const char* ImGui::TableGetColumnName(const ImGuiTable* table, int column_n)
{
    if (table->IsLayoutLocked == false && column_n >= table->DeclColumnsCount)
        return ""; // NameOffset is invalid at this point
    const ImGuiTableColumn* column = &table->Columns[column_n];
    if (column->NameOffset == -1)
        return "";
    return &table->ColumnsNames.Buf[column->NameOffset];
}

// Change user accessible enabled/disabled state of a column (often perceived as "showing/hiding" from users point of view)
// Note that end-user can use the context menu to change this themselves (right-click in headers, or right-click in columns body with ImGuiTableFlags_ContextMenuInBody)
// - Require table to have the ImGuiTableFlags_Hideable flag because we are manipulating user accessible state.
// - Request will be applied during next layout, which happens on the first call to TableNextRow() after BeginTable().
// - For the getter you can test (TableGetColumnFlags() & ImGuiTableColumnFlags_IsEnabled) != 0.
// - Alternative: the ImGuiTableColumnFlags_Disabled is an overriding/master disable flag which will also hide the column from context menu.
void ImGui::TableSetColumnEnabled(int column_n, bool enabled)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL);
    if (!table)
        return;
    IM_ASSERT(table->Flags & ImGuiTableFlags_Hideable); // See comments above
    if (column_n < 0)
        column_n = table->CurrentColumn;
    IM_ASSERT(column_n >= 0 && column_n < table->ColumnsCount);
    ImGuiTableColumn* column = &table->Columns[column_n];
    column->IsUserEnabledNextFrame = enabled;
}

// We allow querying for an extra column in order to poll the IsHovered state of the right-most section
ImGuiTableColumnFlags ImGui::TableGetColumnFlags(int column_n)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return ImGuiTableColumnFlags_None;
    if (column_n < 0)
        column_n = table->CurrentColumn;
    if (column_n == table->ColumnsCount)
        return (table->HoveredColumnBody == column_n) ? ImGuiTableColumnFlags_IsHovered : ImGuiTableColumnFlags_None;
    return table->Columns[column_n].Flags;
}

// Return the cell rectangle based on currently known height.
// - Important: we generally don't know our row height until the end of the row, so Max.y will be incorrect in many situations.
//   The only case where this is correct is if we provided a min_row_height to TableNextRow() and don't go below it, or in TableEndRow() when we locked that height.
// - Important: if ImGuiTableFlags_PadOuterX is set but ImGuiTableFlags_PadInnerX is not set, the outer-most left and right
//   columns report a small offset so their CellBgRect can extend up to the outer border.
//   FIXME: But the rendering code in TableEndRow() nullifies that with clamping required for scrolling.
ImRect ImGui::TableGetCellBgRect(const ImGuiTable* table, int column_n)
{
    const ImGuiTableColumn* column = &table->Columns[column_n];
    float x1 = column->MinX;
    float x2 = column->MaxX;
    //if (column->PrevEnabledColumn == -1)
    //    x1 -= table->OuterPaddingX;
    //if (column->NextEnabledColumn == -1)
    //    x2 += table->OuterPaddingX;
    x1 = ImMax(x1, table->WorkRect.Min.x);
    x2 = ImMin(x2, table->WorkRect.Max.x);
    return ImRect(x1, table->RowPosY1, x2, table->RowPosY2);
}

// Return the resizing ID for the right-side of the given column.
ImGuiID ImGui::TableGetColumnResizeID(const ImGuiTable* table, int column_n, int instance_no)
{
    IM_ASSERT(column_n >= 0 && column_n < table->ColumnsCount);
    ImGuiID id = table->ID + 1 + (instance_no * table->ColumnsCount) + column_n;
    return id;
}

// Return -1 when table is not hovered. return columns_count if the unused space at the right of visible columns is hovered.
int ImGui::TableGetHoveredColumn()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return -1;
    return (int)table->HoveredColumnBody;
}

void ImGui::TableSetBgColor(ImGuiTableBgTarget target, ImU32 color, int column_n)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(target != ImGuiTableBgTarget_None);

    if (color == IM_COL32_DISABLE)
        color = 0;

    // We cannot draw neither the cell or row background immediately as we don't know the row height at this point in time.
    switch (target)
    {
    case ImGuiTableBgTarget_CellBg:
    {
        if (table->RowPosY1 > table->InnerClipRect.Max.y) // Discard
            return;
        if (column_n == -1)
            column_n = table->CurrentColumn;
        if ((table->VisibleMaskByIndex & ((ImU64)1 << column_n)) == 0)
            return;
        if (table->RowCellDataCurrent < 0 || table->RowCellData[table->RowCellDataCurrent].Column != column_n)
            table->RowCellDataCurrent++;
        ImGuiTableCellData* cell_data = &table->RowCellData[table->RowCellDataCurrent];
        cell_data->BgColor = color;
        cell_data->Column = (ImGuiTableColumnIdx)column_n;
        break;
    }
    case ImGuiTableBgTarget_RowBg0:
    case ImGuiTableBgTarget_RowBg1:
    {
        if (table->RowPosY1 > table->InnerClipRect.Max.y) // Discard
            return;
        IM_ASSERT(column_n == -1);
        int bg_idx = (target == ImGuiTableBgTarget_RowBg1) ? 1 : 0;
        table->RowBgColor[bg_idx] = color;
        break;
    }
    default:
        IM_ASSERT(0);
    }
}

//-------------------------------------------------------------------------
// [SECTION] Tables: Row changes
//-------------------------------------------------------------------------
// - TableGetRowIndex()
// - TableNextRow()
// - TableBeginRow() [Internal]
// - TableEndRow() [Internal]
//-------------------------------------------------------------------------

// [Public] Note: for row coloring we use ->RowBgColorCounter which is the same value without counting header rows
int ImGui::TableGetRowIndex()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return 0;
    return table->CurrentRow;
}

// [Public] Starts into the first cell of a new row
void ImGui::TableNextRow(ImGuiTableRowFlags row_flags, float row_min_height)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;

    if (!table->IsLayoutLocked)
        TableUpdateLayout(table);
    if (table->IsInsideRow)
        TableEndRow(table);

    table->LastRowFlags = table->RowFlags;
    table->RowFlags = row_flags;
    table->RowMinHeight = row_min_height;
    TableBeginRow(table);

    // We honor min_row_height requested by user, but cannot guarantee per-row maximum height,
    // because that would essentially require a unique clipping rectangle per-cell.
    table->RowPosY2 += table->CellPaddingY * 2.0f;
    table->RowPosY2 = ImMax(table->RowPosY2, table->RowPosY1 + row_min_height);

    // Disable output until user calls TableNextColumn()
    table->InnerWindow->SkipItems = true;
}

// [Internal] Called by TableNextRow()
void ImGui::TableBeginRow(ImGuiTable* table)
{
    ImGuiWindow* window = table->InnerWindow;
    IM_ASSERT(!table->IsInsideRow);

    // New row
    table->CurrentRow++;
    table->CurrentColumn = -1;
    table->RowBgColor[0] = table->RowBgColor[1] = IM_COL32_DISABLE;
    table->RowCellDataCurrent = -1;
    table->IsInsideRow = true;

    // Begin frozen rows
    float next_y1 = table->RowPosY2;
    if (table->CurrentRow == 0 && table->FreezeRowsCount > 0)
        next_y1 = window->DC.CursorPos.y = table->OuterRect.Min.y;

    table->RowPosY1 = table->RowPosY2 = next_y1;
    table->RowTextBaseline = 0.0f;
    table->RowIndentOffsetX = window->DC.Indent.x - table->HostIndentX; // Lock indent
    window->DC.PrevLineTextBaseOffset = 0.0f;
    window->DC.CursorMaxPos.y = next_y1;

    // Making the header BG color non-transparent will allow us to overlay it multiple times when handling smooth dragging.
    if (table->RowFlags & ImGuiTableRowFlags_Headers)
    {
        TableSetBgColor(ImGuiTableBgTarget_RowBg0, GetColorU32(ImGuiCol_TableHeaderBg));
        if (table->CurrentRow == 0)
            table->IsUsingHeaders = true;
    }
}

// [Internal] Called by TableNextRow()
void ImGui::TableEndRow(ImGuiTable* table)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    IM_ASSERT(window == table->InnerWindow);
    IM_ASSERT(table->IsInsideRow);

    if (table->CurrentColumn != -1)
        TableEndCell(table);

    // Logging
    if (g.LogEnabled)
        LogRenderedText(NULL, "|");

    // Position cursor at the bottom of our row so it can be used for e.g. clipping calculation. However it is
    // likely that the next call to TableBeginCell() will reposition the cursor to take account of vertical padding.
    window->DC.CursorPos.y = table->RowPosY2;

    // Row background fill
    const float bg_y1 = table->RowPosY1;
    const float bg_y2 = table->RowPosY2;
    const bool unfreeze_rows_actual = (table->CurrentRow + 1 == table->FreezeRowsCount);
    const bool unfreeze_rows_request = (table->CurrentRow + 1 == table->FreezeRowsRequest);
    if (table->CurrentRow == 0)
        TableGetInstanceData(table, table->InstanceCurrent)->LastFirstRowHeight = bg_y2 - bg_y1;

    const bool is_visible = (bg_y2 >= table->InnerClipRect.Min.y && bg_y1 <= table->InnerClipRect.Max.y);
    if (is_visible)
    {
        // Decide of background color for the row
        ImU32 bg_col0 = 0;
        ImU32 bg_col1 = 0;
        if (table->RowBgColor[0] != IM_COL32_DISABLE)
            bg_col0 = table->RowBgColor[0];
        else if (table->Flags & ImGuiTableFlags_RowBg)
            bg_col0 = GetColorU32((table->RowBgColorCounter & 1) ? ImGuiCol_TableRowBgAlt : ImGuiCol_TableRowBg);
        if (table->RowBgColor[1] != IM_COL32_DISABLE)
            bg_col1 = table->RowBgColor[1];

        // Decide of top border color
        ImU32 border_col = 0;
        const float border_size = TABLE_BORDER_SIZE;
        if (table->CurrentRow > 0 || table->InnerWindow == table->OuterWindow)
            if (table->Flags & ImGuiTableFlags_BordersInnerH)
                border_col = (table->LastRowFlags & ImGuiTableRowFlags_Headers) ? table->BorderColorStrong : table->BorderColorLight;

        const bool draw_cell_bg_color = table->RowCellDataCurrent >= 0;
        const bool draw_strong_bottom_border = unfreeze_rows_actual;
        if ((bg_col0 | bg_col1 | border_col) != 0 || draw_strong_bottom_border || draw_cell_bg_color)
        {
            // In theory we could call SetWindowClipRectBeforeSetChannel() but since we know TableEndRow() is
            // always followed by a change of clipping rectangle we perform the smallest overwrite possible here.
            if ((table->Flags & ImGuiTableFlags_NoClip) == 0)
                window->DrawList->_CmdHeader.ClipRect = table->Bg0ClipRectForDrawCmd.ToVec4();
            table->DrawSplitter->SetCurrentChannel(window->DrawList, TABLE_DRAW_CHANNEL_BG0);
        }

        // Draw row background
        // We soft/cpu clip this so all backgrounds and borders can share the same clipping rectangle
        if (bg_col0 || bg_col1)
        {
            ImRect row_rect(table->WorkRect.Min.x, bg_y1, table->WorkRect.Max.x, bg_y2);
            row_rect.ClipWith(table->BgClipRect);
            if (bg_col0 != 0 && row_rect.Min.y < row_rect.Max.y)
                window->DrawList->AddRectFilled(row_rect.Min, row_rect.Max, bg_col0);
            if (bg_col1 != 0 && row_rect.Min.y < row_rect.Max.y)
                window->DrawList->AddRectFilled(row_rect.Min, row_rect.Max, bg_col1);
        }

        // Draw cell background color
        if (draw_cell_bg_color)
        {
            ImGuiTableCellData* cell_data_end = &table->RowCellData[table->RowCellDataCurrent];
            for (ImGuiTableCellData* cell_data = &table->RowCellData[0]; cell_data <= cell_data_end; cell_data++)
            {
                // As we render the BG here we need to clip things (for layout we would not)
                // FIXME: This cancels the OuterPadding addition done by TableGetCellBgRect(), need to keep it while rendering correctly while scrolling.
                const ImGuiTableColumn* column = &table->Columns[cell_data->Column];
                ImRect cell_bg_rect = TableGetCellBgRect(table, cell_data->Column);
                cell_bg_rect.ClipWith(table->BgClipRect);
                cell_bg_rect.Min.x = ImMax(cell_bg_rect.Min.x, column->ClipRect.Min.x);     // So that first column after frozen one gets clipped when scrolling
                cell_bg_rect.Max.x = ImMin(cell_bg_rect.Max.x, column->MaxX);
                window->DrawList->AddRectFilled(cell_bg_rect.Min, cell_bg_rect.Max, cell_data->BgColor);
            }
        }

        // Draw top border
        if (border_col && bg_y1 >= table->BgClipRect.Min.y && bg_y1 < table->BgClipRect.Max.y)
            window->DrawList->AddLine(ImVec2(table->BorderX1, bg_y1), ImVec2(table->BorderX2, bg_y1), border_col, border_size);

        // Draw bottom border at the row unfreezing mark (always strong)
        if (draw_strong_bottom_border && bg_y2 >= table->BgClipRect.Min.y && bg_y2 < table->BgClipRect.Max.y)
            window->DrawList->AddLine(ImVec2(table->BorderX1, bg_y2), ImVec2(table->BorderX2, bg_y2), table->BorderColorStrong, border_size);
    }

    // End frozen rows (when we are past the last frozen row line, teleport cursor and alter clipping rectangle)
    // We need to do that in TableEndRow() instead of TableBeginRow() so the list clipper can mark end of row and
    // get the new cursor position.
    if (unfreeze_rows_request)
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
        {
            ImGuiTableColumn* column = &table->Columns[column_n];
            column->NavLayerCurrent = (ImS8)((column_n < table->FreezeColumnsCount) ? ImGuiNavLayer_Menu : ImGuiNavLayer_Main);
        }
    if (unfreeze_rows_actual)
    {
        IM_ASSERT(table->IsUnfrozenRows == false);
        table->IsUnfrozenRows = true;

        // BgClipRect starts as table->InnerClipRect, reduce it now and make BgClipRectForDrawCmd == BgClipRect
        float y0 = ImMax(table->RowPosY2 + 1, window->InnerClipRect.Min.y);
        table->BgClipRect.Min.y = table->Bg2ClipRectForDrawCmd.Min.y = ImMin(y0, window->InnerClipRect.Max.y);
        table->BgClipRect.Max.y = table->Bg2ClipRectForDrawCmd.Max.y = window->InnerClipRect.Max.y;
        table->Bg2DrawChannelCurrent = table->Bg2DrawChannelUnfrozen;
        IM_ASSERT(table->Bg2ClipRectForDrawCmd.Min.y <= table->Bg2ClipRectForDrawCmd.Max.y);

        float row_height = table->RowPosY2 - table->RowPosY1;
        table->RowPosY2 = window->DC.CursorPos.y = table->WorkRect.Min.y + table->RowPosY2 - table->OuterRect.Min.y;
        table->RowPosY1 = table->RowPosY2 - row_height;
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
        {
            ImGuiTableColumn* column = &table->Columns[column_n];
            column->DrawChannelCurrent = column->DrawChannelUnfrozen;
            column->ClipRect.Min.y = table->Bg2ClipRectForDrawCmd.Min.y;
        }

        // Update cliprect ahead of TableBeginCell() so clipper can access to new ClipRect->Min.y
        SetWindowClipRectBeforeSetChannel(window, table->Columns[0].ClipRect);
        table->DrawSplitter->SetCurrentChannel(window->DrawList, table->Columns[0].DrawChannelCurrent);
    }

    if (!(table->RowFlags & ImGuiTableRowFlags_Headers))
        table->RowBgColorCounter++;
    table->IsInsideRow = false;
}

//-------------------------------------------------------------------------
// [SECTION] Tables: Columns changes
//-------------------------------------------------------------------------
// - TableGetColumnIndex()
// - TableSetColumnIndex()
// - TableNextColumn()
// - TableBeginCell() [Internal]
// - TableEndCell() [Internal]
//-------------------------------------------------------------------------

int ImGui::TableGetColumnIndex()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return 0;
    return table->CurrentColumn;
}

// [Public] Append into a specific column
bool ImGui::TableSetColumnIndex(int column_n)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return false;

    if (table->CurrentColumn != column_n)
    {
        if (table->CurrentColumn != -1)
            TableEndCell(table);
        IM_ASSERT(column_n >= 0 && table->ColumnsCount);
        TableBeginCell(table, column_n);
    }

    // Return whether the column is visible. User may choose to skip submitting items based on this return value,
    // however they shouldn't skip submitting for columns that may have the tallest contribution to row height.
    return (table->RequestOutputMaskByIndex & ((ImU64)1 << column_n)) != 0;
}

// [Public] Append into the next column, wrap and create a new row when already on last column
bool ImGui::TableNextColumn()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return false;

    if (table->IsInsideRow && table->CurrentColumn + 1 < table->ColumnsCount)
    {
        if (table->CurrentColumn != -1)
            TableEndCell(table);
        TableBeginCell(table, table->CurrentColumn + 1);
    }
    else
    {
        TableNextRow();
        TableBeginCell(table, 0);
    }

    // Return whether the column is visible. User may choose to skip submitting items based on this return value,
    // however they shouldn't skip submitting for columns that may have the tallest contribution to row height.
    int column_n = table->CurrentColumn;
    return (table->RequestOutputMaskByIndex & ((ImU64)1 << column_n)) != 0;
}


// [Internal] Called by TableSetColumnIndex()/TableNextColumn()
// This is called very frequently, so we need to be mindful of unnecessary overhead.
// FIXME-TABLE FIXME-OPT: Could probably shortcut some things for non-active or clipped columns.
void ImGui::TableBeginCell(ImGuiTable* table, int column_n)
{
    ImGuiTableColumn* column = &table->Columns[column_n];
    ImGuiWindow* window = table->InnerWindow;
    table->CurrentColumn = column_n;

    // Start position is roughly ~~ CellRect.Min + CellPadding + Indent
    float start_x = column->WorkMinX;
    if (column->Flags & ImGuiTableColumnFlags_IndentEnable)
        start_x += table->RowIndentOffsetX; // ~~ += window.DC.Indent.x - table->HostIndentX, except we locked it for the row.

    window->DC.CursorPos.x = start_x;
    window->DC.CursorPos.y = table->RowPosY1 + table->CellPaddingY;
    window->DC.CursorMaxPos.x = window->DC.CursorPos.x;
    window->DC.ColumnsOffset.x = start_x - window->Pos.x - window->DC.Indent.x; // FIXME-WORKRECT
    window->DC.CurrLineTextBaseOffset = table->RowTextBaseline;
    window->DC.NavLayerCurrent = (ImGuiNavLayer)column->NavLayerCurrent;

    window->WorkRect.Min.y = window->DC.CursorPos.y;
    window->WorkRect.Min.x = column->WorkMinX;
    window->WorkRect.Max.x = column->WorkMaxX;
    window->DC.ItemWidth = column->ItemWidth;

    // To allow ImGuiListClipper to function we propagate our row height
    if (!column->IsEnabled)
        window->DC.CursorPos.y = ImMax(window->DC.CursorPos.y, table->RowPosY2);

    window->SkipItems = column->IsSkipItems;
    if (column->IsSkipItems)
    {
        ImGuiContext& g = *GImGui;
        g.LastItemData.ID = 0;
        g.LastItemData.StatusFlags = 0;
    }

    if (table->Flags & ImGuiTableFlags_NoClip)
    {
        // FIXME: if we end up drawing all borders/bg in EndTable, could remove this and just assert that channel hasn't changed.
        table->DrawSplitter->SetCurrentChannel(window->DrawList, TABLE_DRAW_CHANNEL_NOCLIP);
        //IM_ASSERT(table->DrawSplitter._Current == TABLE_DRAW_CHANNEL_NOCLIP);
    }
    else
    {
        // FIXME-TABLE: Could avoid this if draw channel is dummy channel?
        SetWindowClipRectBeforeSetChannel(window, column->ClipRect);
        table->DrawSplitter->SetCurrentChannel(window->DrawList, column->DrawChannelCurrent);
    }

    // Logging
    ImGuiContext& g = *GImGui;
    if (g.LogEnabled && !column->IsSkipItems)
    {
        LogRenderedText(&window->DC.CursorPos, "|");
        g.LogLinePosY = FLT_MAX;
    }
}

// [Internal] Called by TableNextRow()/TableSetColumnIndex()/TableNextColumn()
void ImGui::TableEndCell(ImGuiTable* table)
{
    ImGuiTableColumn* column = &table->Columns[table->CurrentColumn];
    ImGuiWindow* window = table->InnerWindow;

    // Report maximum position so we can infer content size per column.
    float* p_max_pos_x;
    if (table->RowFlags & ImGuiTableRowFlags_Headers)
        p_max_pos_x = &column->ContentMaxXHeadersUsed;  // Useful in case user submit contents in header row that is not a TableHeader() call
    else
        p_max_pos_x = table->IsUnfrozenRows ? &column->ContentMaxXUnfrozen : &column->ContentMaxXFrozen;
    *p_max_pos_x = ImMax(*p_max_pos_x, window->DC.CursorMaxPos.x);
    table->RowPosY2 = ImMax(table->RowPosY2, window->DC.CursorMaxPos.y + table->CellPaddingY);
    column->ItemWidth = window->DC.ItemWidth;

    // Propagate text baseline for the entire row
    // FIXME-TABLE: Here we propagate text baseline from the last line of the cell.. instead of the first one.
    table->RowTextBaseline = ImMax(table->RowTextBaseline, window->DC.PrevLineTextBaseOffset);
}

//-------------------------------------------------------------------------
// [SECTION] Tables: Columns width management
//-------------------------------------------------------------------------
// - TableGetMaxColumnWidth() [Internal]
// - TableGetColumnWidthAuto() [Internal]
// - TableSetColumnWidth()
// - TableSetColumnWidthAutoSingle() [Internal]
// - TableSetColumnWidthAutoAll() [Internal]
// - TableUpdateColumnsWeightFromWidth() [Internal]
//-------------------------------------------------------------------------

// Maximum column content width given current layout. Use column->MinX so this value on a per-column basis.
float ImGui::TableGetMaxColumnWidth(const ImGuiTable* table, int column_n)
{
    const ImGuiTableColumn* column = &table->Columns[column_n];
    float max_width = FLT_MAX;
    const float min_column_distance = table->MinColumnWidth + table->CellPaddingX * 2.0f + table->CellSpacingX1 + table->CellSpacingX2;
    if (table->Flags & ImGuiTableFlags_ScrollX)
    {
        // Frozen columns can't reach beyond visible width else scrolling will naturally break.
        // (we use DisplayOrder as within a set of multiple frozen column reordering is possible)
        if (column->DisplayOrder < table->FreezeColumnsRequest)
        {
            max_width = (table->InnerClipRect.Max.x - (table->FreezeColumnsRequest - column->DisplayOrder) * min_column_distance) - column->MinX;
            max_width = max_width - table->OuterPaddingX - table->CellPaddingX - table->CellSpacingX2;
        }
    }
    else if ((table->Flags & ImGuiTableFlags_NoKeepColumnsVisible) == 0)
    {
        // If horizontal scrolling if disabled, we apply a final lossless shrinking of columns in order to make
        // sure they are all visible. Because of this we also know that all of the columns will always fit in
        // table->WorkRect and therefore in table->InnerRect (because ScrollX is off)
        // FIXME-TABLE: This is solved incorrectly but also quite a difficult problem to fix as we also want ClipRect width to match.
        // See "table_width_distrib" and "table_width_keep_visible" tests
        max_width = table->WorkRect.Max.x - (table->ColumnsEnabledCount - column->IndexWithinEnabledSet - 1) * min_column_distance - column->MinX;
        //max_width -= table->CellSpacingX1;
        max_width -= table->CellSpacingX2;
        max_width -= table->CellPaddingX * 2.0f;
        max_width -= table->OuterPaddingX;
    }
    return max_width;
}

// Note this is meant to be stored in column->WidthAuto, please generally use the WidthAuto field
float ImGui::TableGetColumnWidthAuto(ImGuiTable* table, ImGuiTableColumn* column)
{
    const float content_width_body = ImMax(column->ContentMaxXFrozen, column->ContentMaxXUnfrozen) - column->WorkMinX;
    const float content_width_headers = column->ContentMaxXHeadersIdeal - column->WorkMinX;
    float width_auto = content_width_body;
    if (!(column->Flags & ImGuiTableColumnFlags_NoHeaderWidth))
        width_auto = ImMax(width_auto, content_width_headers);

    // Non-resizable fixed columns preserve their requested width
    if ((column->Flags & ImGuiTableColumnFlags_WidthFixed) && column->InitStretchWeightOrWidth > 0.0f)
        if (!(table->Flags & ImGuiTableFlags_Resizable) || (column->Flags & ImGuiTableColumnFlags_NoResize))
            width_auto = column->InitStretchWeightOrWidth;

    return ImMax(width_auto, table->MinColumnWidth);
}

// 'width' = inner column width, without padding
void ImGui::TableSetColumnWidth(int column_n, float width)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && table->IsLayoutLocked == false);
    IM_ASSERT(column_n >= 0 && column_n < table->ColumnsCount);
    ImGuiTableColumn* column_0 = &table->Columns[column_n];
    float column_0_width = width;

    // Apply constraints early
    // Compare both requested and actual given width to avoid overwriting requested width when column is stuck (minimum size, bounded)
    IM_ASSERT(table->MinColumnWidth > 0.0f);
    const float min_width = table->MinColumnWidth;
    const float max_width = ImMax(min_width, TableGetMaxColumnWidth(table, column_n));
    column_0_width = ImClamp(column_0_width, min_width, max_width);
    if (column_0->WidthGiven == column_0_width || column_0->WidthRequest == column_0_width)
        return;

    //IMGUI_DEBUG_LOG("TableSetColumnWidth(%d, %.1f->%.1f)\n", column_0_idx, column_0->WidthGiven, column_0_width);
    ImGuiTableColumn* column_1 = (column_0->NextEnabledColumn != -1) ? &table->Columns[column_0->NextEnabledColumn] : NULL;

    // In this surprisingly not simple because of how we support mixing Fixed and multiple Stretch columns.
    // - All fixed: easy.
    // - All stretch: easy.
    // - One or more fixed + one stretch: easy.
    // - One or more fixed + more than one stretch: tricky.
    // Qt when manual resize is enabled only support a single _trailing_ stretch column.

    // When forwarding resize from Wn| to Fn+1| we need to be considerate of the _NoResize flag on Fn+1.
    // FIXME-TABLE: Find a way to rewrite all of this so interactions feel more consistent for the user.
    // Scenarios:
    // - F1 F2 F3  resize from F1| or F2|   --> ok: alter ->WidthRequested of Fixed column. Subsequent columns will be offset.
    // - F1 F2 F3  resize from F3|          --> ok: alter ->WidthRequested of Fixed column. If active, ScrollX extent can be altered.
    // - F1 F2 W3  resize from F1| or F2|   --> ok: alter ->WidthRequested of Fixed column. If active, ScrollX extent can be altered, but it doesn't make much sense as the Stretch column will always be minimal size.
    // - F1 F2 W3  resize from W3|          --> ok: no-op (disabled by Resize Rule 1)
    // - W1 W2 W3  resize from W1| or W2|   --> ok
    // - W1 W2 W3  resize from W3|          --> ok: no-op (disabled by Resize Rule 1)
    // - W1 F2 F3  resize from F3|          --> ok: no-op (disabled by Resize Rule 1)
    // - W1 F2     resize from F2|          --> ok: no-op (disabled by Resize Rule 1)
    // - W1 W2 F3  resize from W1| or W2|   --> ok
    // - W1 F2 W3  resize from W1| or F2|   --> ok
    // - F1 W2 F3  resize from W2|          --> ok
    // - F1 W3 F2  resize from W3|          --> ok
    // - W1 F2 F3  resize from W1|          --> ok: equivalent to resizing |F2. F3 will not move.
    // - W1 F2 F3  resize from F2|          --> ok
    // All resizes from a Wx columns are locking other columns.

    // Possible improvements:
    // - W1 W2 W3  resize W1|               --> to not be stuck, both W2 and W3 would stretch down. Seems possible to fix. Would be most beneficial to simplify resize of all-weighted columns.
    // - W3 F1 F2  resize W3|               --> to not be stuck past F1|, both F1 and F2 would need to stretch down, which would be lossy or ambiguous. Seems hard to fix.

    // [Resize Rule 1] Can't resize from right of right-most visible column if there is any Stretch column. Implemented in TableUpdateLayout().

    // If we have all Fixed columns OR resizing a Fixed column that doesn't come after a Stretch one, we can do an offsetting resize.
    // This is the preferred resize path
    if (column_0->Flags & ImGuiTableColumnFlags_WidthFixed)
        if (!column_1 || table->LeftMostStretchedColumn == -1 || table->Columns[table->LeftMostStretchedColumn].DisplayOrder >= column_0->DisplayOrder)
        {
            column_0->WidthRequest = column_0_width;
            table->IsSettingsDirty = true;
            return;
        }

    // We can also use previous column if there's no next one (this is used when doing an auto-fit on the right-most stretch column)
    if (column_1 == NULL)
        column_1 = (column_0->PrevEnabledColumn != -1) ? &table->Columns[column_0->PrevEnabledColumn] : NULL;
    if (column_1 == NULL)
        return;

    // Resizing from right-side of a Stretch column before a Fixed column forward sizing to left-side of fixed column.
    // (old_a + old_b == new_a + new_b) --> (new_a == old_a + old_b - new_b)
    float column_1_width = ImMax(column_1->WidthRequest - (column_0_width - column_0->WidthRequest), min_width);
    column_0_width = column_0->WidthRequest + column_1->WidthRequest - column_1_width;
    IM_ASSERT(column_0_width > 0.0f && column_1_width > 0.0f);
    column_0->WidthRequest = column_0_width;
    column_1->WidthRequest = column_1_width;
    if ((column_0->Flags | column_1->Flags) & ImGuiTableColumnFlags_WidthStretch)
        TableUpdateColumnsWeightFromWidth(table);
    table->IsSettingsDirty = true;
}

// Disable clipping then auto-fit, will take 2 frames
// (we don't take a shortcut for unclipped columns to reduce inconsistencies when e.g. resizing multiple columns)
void ImGui::TableSetColumnWidthAutoSingle(ImGuiTable* table, int column_n)
{
    // Single auto width uses auto-fit
    ImGuiTableColumn* column = &table->Columns[column_n];
    if (!column->IsEnabled)
        return;
    column->CannotSkipItemsQueue = (1 << 0);
    table->AutoFitSingleColumn = (ImGuiTableColumnIdx)column_n;
}

void ImGui::TableSetColumnWidthAutoAll(ImGuiTable* table)
{
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        ImGuiTableColumn* column = &table->Columns[column_n];
        if (!column->IsEnabled && !(column->Flags & ImGuiTableColumnFlags_WidthStretch)) // Cannot reset weight of hidden stretch column
            continue;
        column->CannotSkipItemsQueue = (1 << 0);
        column->AutoFitQueue = (1 << 1);
    }
}

void ImGui::TableUpdateColumnsWeightFromWidth(ImGuiTable* table)
{
    IM_ASSERT(table->LeftMostStretchedColumn != -1 && table->RightMostStretchedColumn != -1);

    // Measure existing quantity
    float visible_weight = 0.0f;
    float visible_width = 0.0f;
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        ImGuiTableColumn* column = &table->Columns[column_n];
        if (!column->IsEnabled || !(column->Flags & ImGuiTableColumnFlags_WidthStretch))
            continue;
        IM_ASSERT(column->StretchWeight > 0.0f);
        visible_weight += column->StretchWeight;
        visible_width += column->WidthRequest;
    }
    IM_ASSERT(visible_weight > 0.0f && visible_width > 0.0f);

    // Apply new weights
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        ImGuiTableColumn* column = &table->Columns[column_n];
        if (!column->IsEnabled || !(column->Flags & ImGuiTableColumnFlags_WidthStretch))
            continue;
        column->StretchWeight = (column->WidthRequest / visible_width) * visible_weight;
        IM_ASSERT(column->StretchWeight > 0.0f);
    }
}

//-------------------------------------------------------------------------
// [SECTION] Tables: Drawing
//-------------------------------------------------------------------------
// - TablePushBackgroundChannel() [Internal]
// - TablePopBackgroundChannel() [Internal]
// - TableSetupDrawChannels() [Internal]
// - TableMergeDrawChannels() [Internal]
// - TableDrawBorders() [Internal]
//-------------------------------------------------------------------------

// Bg2 is used by Selectable (and possibly other widgets) to render to the background.
// Unlike our Bg0/1 channel which we uses for RowBg/CellBg/Borders and where we guarantee all shapes to be CPU-clipped, the Bg2 channel being widgets-facing will rely on regular ClipRect.
void ImGui::TablePushBackgroundChannel()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiTable* table = g.CurrentTable;

    // Optimization: avoid SetCurrentChannel() + PushClipRect()
    table->HostBackupInnerClipRect = window->ClipRect;
    SetWindowClipRectBeforeSetChannel(window, table->Bg2ClipRectForDrawCmd);
    table->DrawSplitter->SetCurrentChannel(window->DrawList, table->Bg2DrawChannelCurrent);
}

void ImGui::TablePopBackgroundChannel()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiTable* table = g.CurrentTable;
    ImGuiTableColumn* column = &table->Columns[table->CurrentColumn];

    // Optimization: avoid PopClipRect() + SetCurrentChannel()
    SetWindowClipRectBeforeSetChannel(window, table->HostBackupInnerClipRect);
    table->DrawSplitter->SetCurrentChannel(window->DrawList, column->DrawChannelCurrent);
}

// Allocate draw channels. Called by TableUpdateLayout()
// - We allocate them following storage order instead of display order so reordering columns won't needlessly
//   increase overall dormant memory cost.
// - We isolate headers draw commands in their own channels instead of just altering clip rects.
//   This is in order to facilitate merging of draw commands.
// - After crossing FreezeRowsCount, all columns see their current draw channel changed to a second set of channels.
// - We only use the dummy draw channel so we can push a null clipping rectangle into it without affecting other
//   channels, while simplifying per-row/per-cell overhead. It will be empty and discarded when merged.
// - We allocate 1 or 2 background draw channels. This is because we know TablePushBackgroundChannel() is only used for
//   horizontal spanning. If we allowed vertical spanning we'd need one background draw channel per merge group (1-4).
// Draw channel allocation (before merging):
// - NoClip                       --> 2+D+1 channels: bg0/1 + bg2 + foreground (same clip rect == always 1 draw call)
// - Clip                         --> 2+D+N channels
// - FreezeRows                   --> 2+D+N*2 (unless scrolling value is zero)
// - FreezeRows || FreezeColunns  --> 3+D+N*2 (unless scrolling value is zero)
// Where D is 1 if any column is clipped or hidden (dummy channel) otherwise 0.
void ImGui::TableSetupDrawChannels(ImGuiTable* table)
{
    const int freeze_row_multiplier = (table->FreezeRowsCount > 0) ? 2 : 1;
    const int channels_for_row = (table->Flags & ImGuiTableFlags_NoClip) ? 1 : table->ColumnsEnabledCount;
    const int channels_for_bg = 1 + 1 * freeze_row_multiplier;
    const int channels_for_dummy = (table->ColumnsEnabledCount < table->ColumnsCount || table->VisibleMaskByIndex != table->EnabledMaskByIndex) ? +1 : 0;
    const int channels_total = channels_for_bg + (channels_for_row * freeze_row_multiplier) + channels_for_dummy;
    table->DrawSplitter->Split(table->InnerWindow->DrawList, channels_total);
    table->DummyDrawChannel = (ImGuiTableDrawChannelIdx)((channels_for_dummy > 0) ? channels_total - 1 : -1);
    table->Bg2DrawChannelCurrent = TABLE_DRAW_CHANNEL_BG2_FROZEN;
    table->Bg2DrawChannelUnfrozen = (ImGuiTableDrawChannelIdx)((table->FreezeRowsCount > 0) ? 2 + channels_for_row : TABLE_DRAW_CHANNEL_BG2_FROZEN);

    int draw_channel_current = 2;
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        ImGuiTableColumn* column = &table->Columns[column_n];
        if (column->IsVisibleX && column->IsVisibleY)
        {
            column->DrawChannelFrozen = (ImGuiTableDrawChannelIdx)(draw_channel_current);
            column->DrawChannelUnfrozen = (ImGuiTableDrawChannelIdx)(draw_channel_current + (table->FreezeRowsCount > 0 ? channels_for_row + 1 : 0));
            if (!(table->Flags & ImGuiTableFlags_NoClip))
                draw_channel_current++;
        }
        else
        {
            column->DrawChannelFrozen = column->DrawChannelUnfrozen = table->DummyDrawChannel;
        }
        column->DrawChannelCurrent = column->DrawChannelFrozen;
    }

    // Initial draw cmd starts with a BgClipRect that matches the one of its host, to facilitate merge draw commands by default.
    // All our cell highlight are manually clipped with BgClipRect. When unfreezing it will be made smaller to fit scrolling rect.
    // (This technically isn't part of setting up draw channels, but is reasonably related to be done here)
    table->BgClipRect = table->InnerClipRect;
    table->Bg0ClipRectForDrawCmd = table->OuterWindow->ClipRect;
    table->Bg2ClipRectForDrawCmd = table->HostClipRect;
    IM_ASSERT(table->BgClipRect.Min.y <= table->BgClipRect.Max.y);
}

// This function reorder draw channels based on matching clip rectangle, to facilitate merging them. Called by EndTable().
// For simplicity we call it TableMergeDrawChannels() but in fact it only reorder channels + overwrite ClipRect,
// actual merging is done by table->DrawSplitter.Merge() which is called right after TableMergeDrawChannels().
//
// Columns where the contents didn't stray off their local clip rectangle can be merged. To achieve
// this we merge their clip rect and make them contiguous in the channel list, so they can be merged
// by the call to DrawSplitter.Merge() following to the call to this function.
// We reorder draw commands by arranging them into a maximum of 4 distinct groups:
//
//   1 group:               2 groups:              2 groups:              4 groups:
//   [ 0. ] no freeze       [ 0. ] row freeze      [ 01 ] col freeze      [ 01 ] row+col freeze
//   [ .. ]  or no scroll   [ 2. ]  and v-scroll   [ .. ]  and h-scroll   [ 23 ]  and v+h-scroll
//
// Each column itself can use 1 channel (row freeze disabled) or 2 channels (row freeze enabled).
// When the contents of a column didn't stray off its limit, we move its channels into the corresponding group
// based on its position (within frozen rows/columns groups or not).
// At the end of the operation our 1-4 groups will each have a ImDrawCmd using the same ClipRect.
// This function assume that each column are pointing to a distinct draw channel,
// otherwise merge_group->ChannelsCount will not match set bit count of merge_group->ChannelsMask.
//
// Column channels will not be merged into one of the 1-4 groups in the following cases:
// - The contents stray off its clipping rectangle (we only compare the MaxX value, not the MinX value).
//   Direct ImDrawList calls won't be taken into account by default, if you use them make sure the ImGui:: bounds
//   matches, by e.g. calling SetCursorScreenPos().
// - The channel uses more than one draw command itself. We drop all our attempt at merging stuff here..
//   we could do better but it's going to be rare and probably not worth the hassle.
// Columns for which the draw channel(s) haven't been merged with other will use their own ImDrawCmd.
//
// This function is particularly tricky to understand.. take a breath.
void ImGui::TableMergeDrawChannels(ImGuiTable* table)
{
    ImGuiContext& g = *GImGui;
    ImDrawListSplitter* splitter = table->DrawSplitter;
    const bool has_freeze_v = (table->FreezeRowsCount > 0);
    const bool has_freeze_h = (table->FreezeColumnsCount > 0);
    IM_ASSERT(splitter->_Current == 0);

    // Track which groups we are going to attempt to merge, and which channels goes into each group.
    struct MergeGroup
    {
        ImRect  ClipRect;
        int     ChannelsCount;
        ImBitArray<IMGUI_TABLE_MAX_DRAW_CHANNELS> ChannelsMask;

        MergeGroup() { ChannelsCount = 0; }
    };
    int merge_group_mask = 0x00;
    MergeGroup merge_groups[4];

    // 1. Scan channels and take note of those which can be merged
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        if ((table->VisibleMaskByIndex & ((ImU64)1 << column_n)) == 0)
            continue;
        ImGuiTableColumn* column = &table->Columns[column_n];

        const int merge_group_sub_count = has_freeze_v ? 2 : 1;
        for (int merge_group_sub_n = 0; merge_group_sub_n < merge_group_sub_count; merge_group_sub_n++)
        {
            const int channel_no = (merge_group_sub_n == 0) ? column->DrawChannelFrozen : column->DrawChannelUnfrozen;

            // Don't attempt to merge if there are multiple draw calls within the column
            ImDrawChannel* src_channel = &splitter->_Channels[channel_no];
            if (src_channel->_CmdBuffer.Size > 0 && src_channel->_CmdBuffer.back().ElemCount == 0 && src_channel->_CmdBuffer.back().UserCallback != NULL) // Equivalent of PopUnusedDrawCmd()
                src_channel->_CmdBuffer.pop_back();
            if (src_channel->_CmdBuffer.Size != 1)
                continue;

            // Find out the width of this merge group and check if it will fit in our column
            // (note that we assume that rendering didn't stray on the left direction. we should need a CursorMinPos to detect it)
            if (!(column->Flags & ImGuiTableColumnFlags_NoClip))
            {
                float content_max_x;
                if (!has_freeze_v)
                    content_max_x = ImMax(column->ContentMaxXUnfrozen, column->ContentMaxXHeadersUsed); // No row freeze
                else if (merge_group_sub_n == 0)
                    content_max_x = ImMax(column->ContentMaxXFrozen, column->ContentMaxXHeadersUsed);   // Row freeze: use width before freeze
                else
                    content_max_x = column->ContentMaxXUnfrozen;                                        // Row freeze: use width after freeze
                if (content_max_x > column->ClipRect.Max.x)
                    continue;
            }

            const int merge_group_n = (has_freeze_h && column_n < table->FreezeColumnsCount ? 0 : 1) + (has_freeze_v && merge_group_sub_n == 0 ? 0 : 2);
            IM_ASSERT(channel_no < IMGUI_TABLE_MAX_DRAW_CHANNELS);
            MergeGroup* merge_group = &merge_groups[merge_group_n];
            if (merge_group->ChannelsCount == 0)
                merge_group->ClipRect = ImRect(+FLT_MAX, +FLT_MAX, -FLT_MAX, -FLT_MAX);
            merge_group->ChannelsMask.SetBit(channel_no);
            merge_group->ChannelsCount++;
            merge_group->ClipRect.Add(src_channel->_CmdBuffer[0].ClipRect);
            merge_group_mask |= (1 << merge_group_n);
        }

        // Invalidate current draw channel
        // (we don't clear DrawChannelFrozen/DrawChannelUnfrozen solely to facilitate debugging/later inspection of data)
        column->DrawChannelCurrent = (ImGuiTableDrawChannelIdx)-1;
    }

    // [DEBUG] Display merge groups
#if 0
    if (g.IO.KeyShift)
        for (int merge_group_n = 0; merge_group_n < IM_ARRAYSIZE(merge_groups); merge_group_n++)
        {
            MergeGroup* merge_group = &merge_groups[merge_group_n];
            if (merge_group->ChannelsCount == 0)
                continue;
            char buf[32];
            ImFormatString(buf, 32, "MG%d:%d", merge_group_n, merge_group->ChannelsCount);
            ImVec2 text_pos = merge_group->ClipRect.Min + ImVec2(4, 4);
            ImVec2 text_size = CalcTextSize(buf, NULL);
            GetForegroundDrawList()->AddRectFilled(text_pos, text_pos + text_size, IM_COL32(0, 0, 0, 255));
            GetForegroundDrawList()->AddText(text_pos, IM_COL32(255, 255, 0, 255), buf, NULL);
            GetForegroundDrawList()->AddRect(merge_group->ClipRect.Min, merge_group->ClipRect.Max, IM_COL32(255, 255, 0, 255));
        }
#endif

    // 2. Rewrite channel list in our preferred order
    if (merge_group_mask != 0)
    {
        // We skip channel 0 (Bg0/Bg1) and 1 (Bg2 frozen) from the shuffling since they won't move - see channels allocation in TableSetupDrawChannels().
        const int LEADING_DRAW_CHANNELS = 2;
        g.DrawChannelsTempMergeBuffer.resize(splitter->_Count - LEADING_DRAW_CHANNELS); // Use shared temporary storage so the allocation gets amortized
        ImDrawChannel* dst_tmp = g.DrawChannelsTempMergeBuffer.Data;
        ImBitArray<IMGUI_TABLE_MAX_DRAW_CHANNELS> remaining_mask;                       // We need 132-bit of storage
        remaining_mask.SetBitRange(LEADING_DRAW_CHANNELS, splitter->_Count);
        remaining_mask.ClearBit(table->Bg2DrawChannelUnfrozen);
        IM_ASSERT(has_freeze_v == false || table->Bg2DrawChannelUnfrozen != TABLE_DRAW_CHANNEL_BG2_FROZEN);
        int remaining_count = splitter->_Count - (has_freeze_v ? LEADING_DRAW_CHANNELS + 1 : LEADING_DRAW_CHANNELS);
        //ImRect host_rect = (table->InnerWindow == table->OuterWindow) ? table->InnerClipRect : table->HostClipRect;
        ImRect host_rect = table->HostClipRect;
        for (int merge_group_n = 0; merge_group_n < IM_ARRAYSIZE(merge_groups); merge_group_n++)
        {
            if (int merge_channels_count = merge_groups[merge_group_n].ChannelsCount)
            {
                MergeGroup* merge_group = &merge_groups[merge_group_n];
                ImRect merge_clip_rect = merge_group->ClipRect;

                // Extend outer-most clip limits to match those of host, so draw calls can be merged even if
                // outer-most columns have some outer padding offsetting them from their parent ClipRect.
                // The principal cases this is dealing with are:
                // - On a same-window table (not scrolling = single group), all fitting columns ClipRect -> will extend and match host ClipRect -> will merge
                // - Columns can use padding and have left-most ClipRect.Min.x and right-most ClipRect.Max.x != from host ClipRect -> will extend and match host ClipRect -> will merge
                // FIXME-TABLE FIXME-WORKRECT: We are wasting a merge opportunity on tables without scrolling if column doesn't fit
                // within host clip rect, solely because of the half-padding difference between window->WorkRect and window->InnerClipRect.
                if ((merge_group_n & 1) == 0 || !has_freeze_h)
                    merge_clip_rect.Min.x = ImMin(merge_clip_rect.Min.x, host_rect.Min.x);
                if ((merge_group_n & 2) == 0 || !has_freeze_v)
                    merge_clip_rect.Min.y = ImMin(merge_clip_rect.Min.y, host_rect.Min.y);
                if ((merge_group_n & 1) != 0)
                    merge_clip_rect.Max.x = ImMax(merge_clip_rect.Max.x, host_rect.Max.x);
                if ((merge_group_n & 2) != 0 && (table->Flags & ImGuiTableFlags_NoHostExtendY) == 0)
                    merge_clip_rect.Max.y = ImMax(merge_clip_rect.Max.y, host_rect.Max.y);
#if 0
                GetOverlayDrawList()->AddRect(merge_group->ClipRect.Min, merge_group->ClipRect.Max, IM_COL32(255, 0, 0, 200), 0.0f, 0, 1.0f);
                GetOverlayDrawList()->AddLine(merge_group->ClipRect.Min, merge_clip_rect.Min, IM_COL32(255, 100, 0, 200));
                GetOverlayDrawList()->AddLine(merge_group->ClipRect.Max, merge_clip_rect.Max, IM_COL32(255, 100, 0, 200));
#endif
                remaining_count -= merge_group->ChannelsCount;
                for (int n = 0; n < IM_ARRAYSIZE(remaining_mask.Storage); n++)
                    remaining_mask.Storage[n] &= ~merge_group->ChannelsMask.Storage[n];
                for (int n = 0; n < splitter->_Count && merge_channels_count != 0; n++)
                {
                    // Copy + overwrite new clip rect
                    if (!merge_group->ChannelsMask.TestBit(n))
                        continue;
                    merge_group->ChannelsMask.ClearBit(n);
                    merge_channels_count--;

                    ImDrawChannel* channel = &splitter->_Channels[n];
                    IM_ASSERT(channel->_CmdBuffer.Size == 1 && merge_clip_rect.Contains(ImRect(channel->_CmdBuffer[0].ClipRect)));
                    channel->_CmdBuffer[0].ClipRect = merge_clip_rect.ToVec4();
                    memcpy(dst_tmp++, channel, sizeof(ImDrawChannel));
                }
            }

            // Make sure Bg2DrawChannelUnfrozen appears in the middle of our groups (whereas Bg0/Bg1 and Bg2 frozen are fixed to 0 and 1)
            if (merge_group_n == 1 && has_freeze_v)
                memcpy(dst_tmp++, &splitter->_Channels[table->Bg2DrawChannelUnfrozen], sizeof(ImDrawChannel));
        }

        // Append unmergeable channels that we didn't reorder at the end of the list
        for (int n = 0; n < splitter->_Count && remaining_count != 0; n++)
        {
            if (!remaining_mask.TestBit(n))
                continue;
            ImDrawChannel* channel = &splitter->_Channels[n];
            memcpy(dst_tmp++, channel, sizeof(ImDrawChannel));
            remaining_count--;
        }
        IM_ASSERT(dst_tmp == g.DrawChannelsTempMergeBuffer.Data + g.DrawChannelsTempMergeBuffer.Size);
        memcpy(splitter->_Channels.Data + LEADING_DRAW_CHANNELS, g.DrawChannelsTempMergeBuffer.Data, (splitter->_Count - LEADING_DRAW_CHANNELS) * sizeof(ImDrawChannel));
    }
}

// FIXME-TABLE: This is a mess, need to redesign how we render borders (as some are also done in TableEndRow)
void ImGui::TableDrawBorders(ImGuiTable* table)
{
    ImGuiWindow* inner_window = table->InnerWindow;
    if (!table->OuterWindow->ClipRect.Overlaps(table->OuterRect))
        return;

    ImDrawList* inner_drawlist = inner_window->DrawList;
    table->DrawSplitter->SetCurrentChannel(inner_drawlist, TABLE_DRAW_CHANNEL_BG0);
    inner_drawlist->PushClipRect(table->Bg0ClipRectForDrawCmd.Min, table->Bg0ClipRectForDrawCmd.Max, false);

    // Draw inner border and resizing feedback
    ImGuiTableInstanceData* table_instance = TableGetInstanceData(table, table->InstanceCurrent);
    const float border_size = TABLE_BORDER_SIZE;
    const float draw_y1 = table->InnerRect.Min.y;
    const float draw_y2_body = table->InnerRect.Max.y;
    const float draw_y2_head = table->IsUsingHeaders ? ImMin(table->InnerRect.Max.y, (table->FreezeRowsCount >= 1 ? table->InnerRect.Min.y : table->WorkRect.Min.y) + table_instance->LastFirstRowHeight) : draw_y1;
    if (table->Flags & ImGuiTableFlags_BordersInnerV)
    {
        for (int order_n = 0; order_n < table->ColumnsCount; order_n++)
        {
            if (!(table->EnabledMaskByDisplayOrder & ((ImU64)1 << order_n)))
                continue;

            const int column_n = table->DisplayOrderToIndex[order_n];
            ImGuiTableColumn* column = &table->Columns[column_n];
            const bool is_hovered = (table->HoveredColumnBorder == column_n);
            const bool is_resized = (table->ResizedColumn == column_n) && (table->InstanceInteracted == table->InstanceCurrent);
            const bool is_resizable = (column->Flags & (ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoDirectResize_)) == 0;
            const bool is_frozen_separator = (table->FreezeColumnsCount == order_n + 1);
            if (column->MaxX > table->InnerClipRect.Max.x && !is_resized)
                continue;

            // Decide whether right-most column is visible
            if (column->NextEnabledColumn == -1 && !is_resizable)
                if ((table->Flags & ImGuiTableFlags_SizingMask_) != ImGuiTableFlags_SizingFixedSame || (table->Flags & ImGuiTableFlags_NoHostExtendX))
                    continue;
            if (column->MaxX <= column->ClipRect.Min.x) // FIXME-TABLE FIXME-STYLE: Assume BorderSize==1, this is problematic if we want to increase the border size..
                continue;

            // Draw in outer window so right-most column won't be clipped
            // Always draw full height border when being resized/hovered, or on the delimitation of frozen column scrolling.
            ImU32 col;
            float draw_y2;
            if (is_hovered || is_resized || is_frozen_separator)
            {
                draw_y2 = draw_y2_body;
                col = is_resized ? GetColorU32(ImGuiCol_SeparatorActive) : is_hovered ? GetColorU32(ImGuiCol_SeparatorHovered) : table->BorderColorStrong;
            }
            else
            {
                draw_y2 = (table->Flags & (ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoBordersInBodyUntilResize)) ? draw_y2_head : draw_y2_body;
                col = (table->Flags & (ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoBordersInBodyUntilResize)) ? table->BorderColorStrong : table->BorderColorLight;
            }

            if (draw_y2 > draw_y1)
                inner_drawlist->AddLine(ImVec2(column->MaxX, draw_y1), ImVec2(column->MaxX, draw_y2), col, border_size);
        }
    }

    // Draw outer border
    // FIXME: could use AddRect or explicit VLine/HLine helper?
    if (table->Flags & ImGuiTableFlags_BordersOuter)
    {
        // Display outer border offset by 1 which is a simple way to display it without adding an extra draw call
        // (Without the offset, in outer_window it would be rendered behind cells, because child windows are above their
        // parent. In inner_window, it won't reach out over scrollbars. Another weird solution would be to display part
        // of it in inner window, and the part that's over scrollbars in the outer window..)
        // Either solution currently won't allow us to use a larger border size: the border would clipped.
        const ImRect outer_border = table->OuterRect;
        const ImU32 outer_col = table->BorderColorStrong;
        if ((table->Flags & ImGuiTableFlags_BordersOuter) == ImGuiTableFlags_BordersOuter)
        {
            inner_drawlist->AddRect(outer_border.Min, outer_border.Max, outer_col, 0.0f, 0, border_size);
        }
        else if (table->Flags & ImGuiTableFlags_BordersOuterV)
        {
            inner_drawlist->AddLine(outer_border.Min, ImVec2(outer_border.Min.x, outer_border.Max.y), outer_col, border_size);
            inner_drawlist->AddLine(ImVec2(outer_border.Max.x, outer_border.Min.y), outer_border.Max, outer_col, border_size);
        }
        else if (table->Flags & ImGuiTableFlags_BordersOuterH)
        {
            inner_drawlist->AddLine(outer_border.Min, ImVec2(outer_border.Max.x, outer_border.Min.y), outer_col, border_size);
            inner_drawlist->AddLine(ImVec2(outer_border.Min.x, outer_border.Max.y), outer_border.Max, outer_col, border_size);
        }
    }
    if ((table->Flags & ImGuiTableFlags_BordersInnerH) && table->RowPosY2 < table->OuterRect.Max.y)
    {
        // Draw bottom-most row border
        const float border_y = table->RowPosY2;
        if (border_y >= table->BgClipRect.Min.y && border_y < table->BgClipRect.Max.y)
            inner_drawlist->AddLine(ImVec2(table->BorderX1, border_y), ImVec2(table->BorderX2, border_y), table->BorderColorLight, border_size);
    }

    inner_drawlist->PopClipRect();
}

//-------------------------------------------------------------------------
// [SECTION] Tables: Sorting
//-------------------------------------------------------------------------
// - TableGetSortSpecs()
// - TableFixColumnSortDirection() [Internal]
// - TableGetColumnNextSortDirection() [Internal]
// - TableSetColumnSortDirection() [Internal]
// - TableSortSpecsSanitize() [Internal]
// - TableSortSpecsBuild() [Internal]
//-------------------------------------------------------------------------

// Return NULL if no sort specs (most often when ImGuiTableFlags_Sortable is not set)
// You can sort your data again when 'SpecsChanged == true'. It will be true with sorting specs have changed since
// last call, or the first time.
// Lifetime: don't hold on this pointer over multiple frames or past any subsequent call to BeginTable()!
ImGuiTableSortSpecs* ImGui::TableGetSortSpecs()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL);

    if (!(table->Flags & ImGuiTableFlags_Sortable))
        return NULL;

    // Require layout (in case TableHeadersRow() hasn't been called) as it may alter IsSortSpecsDirty in some paths.
    if (!table->IsLayoutLocked)
        TableUpdateLayout(table);

    TableSortSpecsBuild(table);

    return &table->SortSpecs;
}

static inline ImGuiSortDirection TableGetColumnAvailSortDirection(ImGuiTableColumn* column, int n)
{
    IM_ASSERT(n < column->SortDirectionsAvailCount);
    return (column->SortDirectionsAvailList >> (n << 1)) & 0x03;
}

// Fix sort direction if currently set on a value which is unavailable (e.g. activating NoSortAscending/NoSortDescending)
void ImGui::TableFixColumnSortDirection(ImGuiTable* table, ImGuiTableColumn* column)
{
    if (column->SortOrder == -1 || (column->SortDirectionsAvailMask & (1 << column->SortDirection)) != 0)
        return;
    column->SortDirection = (ImU8)TableGetColumnAvailSortDirection(column, 0);
    table->IsSortSpecsDirty = true;
}

// Calculate next sort direction that would be set after clicking the column
// - If the PreferSortDescending flag is set, we will default to a Descending direction on the first click.
// - Note that the PreferSortAscending flag is never checked, it is essentially the default and therefore a no-op.
IM_STATIC_ASSERT(ImGuiSortDirection_None == 0 && ImGuiSortDirection_Ascending == 1 && ImGuiSortDirection_Descending == 2);
ImGuiSortDirection ImGui::TableGetColumnNextSortDirection(ImGuiTableColumn* column)
{
    IM_ASSERT(column->SortDirectionsAvailCount > 0);
    if (column->SortOrder == -1)
        return TableGetColumnAvailSortDirection(column, 0);
    for (int n = 0; n < 3; n++)
        if (column->SortDirection == TableGetColumnAvailSortDirection(column, n))
            return TableGetColumnAvailSortDirection(column, (n + 1) % column->SortDirectionsAvailCount);
    IM_ASSERT(0);
    return ImGuiSortDirection_None;
}

// Note that the NoSortAscending/NoSortDescending flags are processed in TableSortSpecsSanitize(), and they may change/revert
// the value of SortDirection. We could technically also do it here but it would be unnecessary and duplicate code.
void ImGui::TableSetColumnSortDirection(int column_n, ImGuiSortDirection sort_direction, bool append_to_sort_specs)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;

    if (!(table->Flags & ImGuiTableFlags_SortMulti))
        append_to_sort_specs = false;
    if (!(table->Flags & ImGuiTableFlags_SortTristate))
        IM_ASSERT(sort_direction != ImGuiSortDirection_None);

    ImGuiTableColumnIdx sort_order_max = 0;
    if (append_to_sort_specs)
        for (int other_column_n = 0; other_column_n < table->ColumnsCount; other_column_n++)
            sort_order_max = ImMax(sort_order_max, table->Columns[other_column_n].SortOrder);

    ImGuiTableColumn* column = &table->Columns[column_n];
    column->SortDirection = (ImU8)sort_direction;
    if (column->SortDirection == ImGuiSortDirection_None)
        column->SortOrder = -1;
    else if (column->SortOrder == -1 || !append_to_sort_specs)
        column->SortOrder = append_to_sort_specs ? sort_order_max + 1 : 0;

    for (int other_column_n = 0; other_column_n < table->ColumnsCount; other_column_n++)
    {
        ImGuiTableColumn* other_column = &table->Columns[other_column_n];
        if (other_column != column && !append_to_sort_specs)
            other_column->SortOrder = -1;
        TableFixColumnSortDirection(table, other_column);
    }
    table->IsSettingsDirty = true;
    table->IsSortSpecsDirty = true;
}

void ImGui::TableSortSpecsSanitize(ImGuiTable* table)
{
    IM_ASSERT(table->Flags & ImGuiTableFlags_Sortable);

    // Clear SortOrder from hidden column and verify that there's no gap or duplicate.
    int sort_order_count = 0;
    ImU64 sort_order_mask = 0x00;
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        ImGuiTableColumn* column = &table->Columns[column_n];
        if (column->SortOrder != -1 && !column->IsEnabled)
            column->SortOrder = -1;
        if (column->SortOrder == -1)
            continue;
        sort_order_count++;
        sort_order_mask |= ((ImU64)1 << column->SortOrder);
        IM_ASSERT(sort_order_count < (int)sizeof(sort_order_mask) * 8);
    }

    const bool need_fix_linearize = ((ImU64)1 << sort_order_count) != (sort_order_mask + 1);
    const bool need_fix_single_sort_order = (sort_order_count > 1) && !(table->Flags & ImGuiTableFlags_SortMulti);
    if (need_fix_linearize || need_fix_single_sort_order)
    {
        ImU64 fixed_mask = 0x00;
        for (int sort_n = 0; sort_n < sort_order_count; sort_n++)
        {
            // Fix: Rewrite sort order fields if needed so they have no gap or duplicate.
            // (e.g. SortOrder 0 disappeared, SortOrder 1..2 exists --> rewrite then as SortOrder 0..1)
            int column_with_smallest_sort_order = -1;
            for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
                if ((fixed_mask & ((ImU64)1 << (ImU64)column_n)) == 0 && table->Columns[column_n].SortOrder != -1)
                    if (column_with_smallest_sort_order == -1 || table->Columns[column_n].SortOrder < table->Columns[column_with_smallest_sort_order].SortOrder)
                        column_with_smallest_sort_order = column_n;
            IM_ASSERT(column_with_smallest_sort_order != -1);
            fixed_mask |= ((ImU64)1 << column_with_smallest_sort_order);
            table->Columns[column_with_smallest_sort_order].SortOrder = (ImGuiTableColumnIdx)sort_n;

            // Fix: Make sure only one column has a SortOrder if ImGuiTableFlags_MultiSortable is not set.
            if (need_fix_single_sort_order)
            {
                sort_order_count = 1;
                for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
                    if (column_n != column_with_smallest_sort_order)
                        table->Columns[column_n].SortOrder = -1;
                break;
            }
        }
    }

    // Fallback default sort order (if no column had the ImGuiTableColumnFlags_DefaultSort flag)
    if (sort_order_count == 0 && !(table->Flags & ImGuiTableFlags_SortTristate))
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
        {
            ImGuiTableColumn* column = &table->Columns[column_n];
            if (column->IsEnabled && !(column->Flags & ImGuiTableColumnFlags_NoSort))
            {
                sort_order_count = 1;
                column->SortOrder = 0;
                column->SortDirection = (ImU8)TableGetColumnAvailSortDirection(column, 0);
                break;
            }
        }

    table->SortSpecsCount = (ImGuiTableColumnIdx)sort_order_count;
}

void ImGui::TableSortSpecsBuild(ImGuiTable* table)
{
    bool dirty = table->IsSortSpecsDirty;
    if (dirty)
    {
        TableSortSpecsSanitize(table);
        table->SortSpecsMulti.resize(table->SortSpecsCount <= 1 ? 0 : table->SortSpecsCount);
        table->SortSpecs.SpecsDirty = true; // Mark as dirty for user
        table->IsSortSpecsDirty = false; // Mark as not dirty for us
    }

    // Write output
    ImGuiTableColumnSortSpecs* sort_specs = (table->SortSpecsCount == 0) ? NULL : (table->SortSpecsCount == 1) ? &table->SortSpecsSingle : table->SortSpecsMulti.Data;
    if (dirty && sort_specs != NULL)
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
        {
            ImGuiTableColumn* column = &table->Columns[column_n];
            if (column->SortOrder == -1)
                continue;
            IM_ASSERT(column->SortOrder < table->SortSpecsCount);
            ImGuiTableColumnSortSpecs* sort_spec = &sort_specs[column->SortOrder];
            sort_spec->ColumnUserID = column->UserID;
            sort_spec->ColumnIndex = (ImGuiTableColumnIdx)column_n;
            sort_spec->SortOrder = (ImGuiTableColumnIdx)column->SortOrder;
            sort_spec->SortDirection = column->SortDirection;
        }

    table->SortSpecs.Specs = sort_specs;
    table->SortSpecs.SpecsCount = table->SortSpecsCount;
}

//-------------------------------------------------------------------------
// [SECTION] Tables: Headers
//-------------------------------------------------------------------------
// - TableGetHeaderRowHeight() [Internal]
// - TableHeadersRow()
// - TableHeader()
//-------------------------------------------------------------------------

float ImGui::TableGetHeaderRowHeight()
{
    // Caring for a minor edge case:
    // Calculate row height, for the unlikely case that some labels may be taller than others.
    // If we didn't do that, uneven header height would highlight but smaller one before the tallest wouldn't catch input for all height.
    // In your custom header row you may omit this all together and just call TableNextRow() without a height...
    float row_height = GetTextLineHeight();
    int columns_count = TableGetColumnCount();
    for (int column_n = 0; column_n < columns_count; column_n++)
    {
        ImGuiTableColumnFlags flags = TableGetColumnFlags(column_n);
        if ((flags & ImGuiTableColumnFlags_IsEnabled) && !(flags & ImGuiTableColumnFlags_NoHeaderLabel))
            row_height = ImMax(row_height, CalcTextSize(TableGetColumnName(column_n)).y);
    }
    row_height += GetStyle().CellPadding.y * 2.0f;
    return row_height;
}

// [Public] This is a helper to output TableHeader() calls based on the column names declared in TableSetupColumn().
// The intent is that advanced users willing to create customized headers would not need to use this helper
// and can create their own! For example: TableHeader() may be preceeded by Checkbox() or other custom widgets.
// See 'Demo->Tables->Custom headers' for a demonstration of implementing a custom version of this.
// This code is constructed to not make much use of internal functions, as it is intended to be a template to copy.
// FIXME-TABLE: TableOpenContextMenu() and TableGetHeaderRowHeight() are not public.
void ImGui::TableHeadersRow()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && "Need to call TableHeadersRow() after BeginTable()!");

    // Layout if not already done (this is automatically done by TableNextRow, we do it here solely to facilitate stepping in debugger as it is frequent to step in TableUpdateLayout)
    if (!table->IsLayoutLocked)
        TableUpdateLayout(table);

    // Open row
    const float row_y1 = GetCursorScreenPos().y;
    const float row_height = TableGetHeaderRowHeight();
    TableNextRow(ImGuiTableRowFlags_Headers, row_height);
    if (table->HostSkipItems) // Merely an optimization, you may skip in your own code.
        return;

    const int columns_count = TableGetColumnCount();
    for (int column_n = 0; column_n < columns_count; column_n++)
    {
        if (!TableSetColumnIndex(column_n))
            continue;

        // Push an id to allow unnamed labels (generally accidental, but let's behave nicely with them)
        // - in your own code you may omit the PushID/PopID all-together, provided you know they won't collide
        // - table->InstanceCurrent is only >0 when we use multiple BeginTable/EndTable calls with same identifier.
        const char* name = (TableGetColumnFlags(column_n) & ImGuiTableColumnFlags_NoHeaderLabel) ? "" : TableGetColumnName(column_n);
        PushID(table->InstanceCurrent * table->ColumnsCount + column_n);
        TableHeader(name);
        PopID();
    }

    // Allow opening popup from the right-most section after the last column.
    ImVec2 mouse_pos = ImGui::GetMousePos();
    if (IsMouseReleased(1) && TableGetHoveredColumn() == columns_count)
        if (mouse_pos.y >= row_y1 && mouse_pos.y < row_y1 + row_height)
            TableOpenContextMenu(-1); // Will open a non-column-specific popup.
}

// Emit a column header (text + optional sort order)
// We cpu-clip text here so that all columns headers can be merged into a same draw call.
// Note that because of how we cpu-clip and display sorting indicators, you _cannot_ use SameLine() after a TableHeader()
void ImGui::TableHeader(const char* label)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return;

    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && "Need to call TableHeader() after BeginTable()!");
    IM_ASSERT(table->CurrentColumn != -1);
    const int column_n = table->CurrentColumn;
    ImGuiTableColumn* column = &table->Columns[column_n];

    // Label
    if (label == NULL)
        label = "";
    const char* label_end = FindRenderedTextEnd(label);
    ImVec2 label_size = CalcTextSize(label, label_end, true);
    ImVec2 label_pos = window->DC.CursorPos;

    // If we already got a row height, there's use that.
    // FIXME-TABLE: Padding problem if the correct outer-padding CellBgRect strays off our ClipRect?
    ImRect cell_r = TableGetCellBgRect(table, column_n);
    float label_height = ImMax(label_size.y, table->RowMinHeight - table->CellPaddingY * 2.0f);

    // Calculate ideal size for sort order arrow
    float w_arrow = 0.0f;
    float w_sort_text = 0.0f;
    char sort_order_suf[4] = "";
    const float ARROW_SCALE = 0.65f;
    if ((table->Flags & ImGuiTableFlags_Sortable) && !(column->Flags & ImGuiTableColumnFlags_NoSort))
    {
        w_arrow = ImFloor(g.FontSize * ARROW_SCALE + g.Style.FramePadding.x);
        if (column->SortOrder > 0)
        {
            ImFormatString(sort_order_suf, IM_ARRAYSIZE(sort_order_suf), "%d", column->SortOrder + 1);
            w_sort_text = g.Style.ItemInnerSpacing.x + CalcTextSize(sort_order_suf).x;
        }
    }

    // We feed our unclipped width to the column without writing on CursorMaxPos, so that column is still considering for merging.
    float max_pos_x = label_pos.x + label_size.x + w_sort_text + w_arrow;
    column->ContentMaxXHeadersUsed = ImMax(column->ContentMaxXHeadersUsed, column->WorkMaxX);
    column->ContentMaxXHeadersIdeal = ImMax(column->ContentMaxXHeadersIdeal, max_pos_x);

    // Keep header highlighted when context menu is open.
    const bool selected = (table->IsContextPopupOpen && table->ContextPopupColumn == column_n && table->InstanceInteracted == table->InstanceCurrent);
    ImGuiID id = window->GetID(label);
    ImRect bb(cell_r.Min.x, cell_r.Min.y, cell_r.Max.x, ImMax(cell_r.Max.y, cell_r.Min.y + label_height + g.Style.CellPadding.y * 2.0f));
    ItemSize(ImVec2(0.0f, label_height)); // Don't declare unclipped width, it'll be fed ContentMaxPosHeadersIdeal
    if (!ItemAdd(bb, id))
        return;

    //GetForegroundDrawList()->AddRect(cell_r.Min, cell_r.Max, IM_COL32(255, 0, 0, 255)); // [DEBUG]
    //GetForegroundDrawList()->AddRect(bb.Min, bb.Max, IM_COL32(255, 0, 0, 255)); // [DEBUG]

    // Using AllowItemOverlap mode because we cover the whole cell, and we want user to be able to submit subsequent items.
    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_AllowItemOverlap);
    if (g.ActiveId != id)
        SetItemAllowOverlap();
    if (held || hovered || selected)
    {
        const ImU32 col = GetColorU32(held ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
        //RenderFrame(bb.Min, bb.Max, col, false, 0.0f);
        TableSetBgColor(ImGuiTableBgTarget_CellBg, col, table->CurrentColumn);
    }
    else
    {
        // Submit single cell bg color in the case we didn't submit a full header row
        if ((table->RowFlags & ImGuiTableRowFlags_Headers) == 0)
            TableSetBgColor(ImGuiTableBgTarget_CellBg, GetColorU32(ImGuiCol_TableHeaderBg), table->CurrentColumn);
    }
    RenderNavHighlight(bb, id, ImGuiNavHighlightFlags_TypeThin | ImGuiNavHighlightFlags_NoRounding);
    if (held)
        table->HeldHeaderColumn = (ImGuiTableColumnIdx)column_n;
    window->DC.CursorPos.y -= g.Style.ItemSpacing.y * 0.5f;

    // Drag and drop to re-order columns.
    // FIXME-TABLE: Scroll request while reordering a column and it lands out of the scrolling zone.
    if (held && (table->Flags & ImGuiTableFlags_Reorderable) && IsMouseDragging(0) && !g.DragDropActive)
    {
        // While moving a column it will jump on the other side of the mouse, so we also test for MouseDelta.x
        table->ReorderColumn = (ImGuiTableColumnIdx)column_n;
        table->InstanceInteracted = table->InstanceCurrent;

        // We don't reorder: through the frozen<>unfrozen line, or through a column that is marked with ImGuiTableColumnFlags_NoReorder.
        if (g.IO.MouseDelta.x < 0.0f && g.IO.MousePos.x < cell_r.Min.x)
            if (ImGuiTableColumn* prev_column = (column->PrevEnabledColumn != -1) ? &table->Columns[column->PrevEnabledColumn] : NULL)
                if (!((column->Flags | prev_column->Flags) & ImGuiTableColumnFlags_NoReorder))
                    if ((column->IndexWithinEnabledSet < table->FreezeColumnsRequest) == (prev_column->IndexWithinEnabledSet < table->FreezeColumnsRequest))
                        table->ReorderColumnDir = -1;
        if (g.IO.MouseDelta.x > 0.0f && g.IO.MousePos.x > cell_r.Max.x)
            if (ImGuiTableColumn* next_column = (column->NextEnabledColumn != -1) ? &table->Columns[column->NextEnabledColumn] : NULL)
                if (!((column->Flags | next_column->Flags) & ImGuiTableColumnFlags_NoReorder))
                    if ((column->IndexWithinEnabledSet < table->FreezeColumnsRequest) == (next_column->IndexWithinEnabledSet < table->FreezeColumnsRequest))
                        table->ReorderColumnDir = +1;
    }

    // Sort order arrow
    const float ellipsis_max = cell_r.Max.x - w_arrow - w_sort_text;
    if ((table->Flags & ImGuiTableFlags_Sortable) && !(column->Flags & ImGuiTableColumnFlags_NoSort))
    {
        if (column->SortOrder != -1)
        {
            float x = ImMax(cell_r.Min.x, cell_r.Max.x - w_arrow - w_sort_text);
            float y = label_pos.y;
            if (column->SortOrder > 0)
            {
                PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_Text, 0.70f));
                RenderText(ImVec2(x + g.Style.ItemInnerSpacing.x, y), sort_order_suf);
                PopStyleColor();
                x += w_sort_text;
            }
            RenderArrow(window->DrawList, ImVec2(x, y), GetColorU32(ImGuiCol_Text), column->SortDirection == ImGuiSortDirection_Ascending ? ImGuiDir_Up : ImGuiDir_Down, ARROW_SCALE);
        }

        // Handle clicking on column header to adjust Sort Order
        if (pressed && table->ReorderColumn != column_n)
        {
            ImGuiSortDirection sort_direction = TableGetColumnNextSortDirection(column);
            TableSetColumnSortDirection(column_n, sort_direction, g.IO.KeyShift);
        }
    }

    // Render clipped label. Clipping here ensure that in the majority of situations, all our header cells will
    // be merged into a single draw call.
    //window->DrawList->AddCircleFilled(ImVec2(ellipsis_max, label_pos.y), 40, IM_COL32_WHITE);
    RenderTextEllipsis(window->DrawList, label_pos, ImVec2(ellipsis_max, label_pos.y + label_height + g.Style.FramePadding.y), ellipsis_max, ellipsis_max, label, label_end, &label_size);

    const bool text_clipped = label_size.x > (ellipsis_max - label_pos.x);
    if (text_clipped && hovered && g.HoveredIdNotActiveTimer > g.TooltipSlowDelay)
        SetTooltip("%.*s", (int)(label_end - label), label);

    // We don't use BeginPopupContextItem() because we want the popup to stay up even after the column is hidden
    if (IsMouseReleased(1) && IsItemHovered())
        TableOpenContextMenu(column_n);
}

//-------------------------------------------------------------------------
// [SECTION] Tables: Context Menu
//-------------------------------------------------------------------------
// - TableOpenContextMenu() [Internal]
// - TableDrawContextMenu() [Internal]
//-------------------------------------------------------------------------

// Use -1 to open menu not specific to a given column.
void ImGui::TableOpenContextMenu(int column_n)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (column_n == -1 && table->CurrentColumn != -1)   // When called within a column automatically use this one (for consistency)
        column_n = table->CurrentColumn;
    if (column_n == table->ColumnsCount)                // To facilitate using with TableGetHoveredColumn()
        column_n = -1;
    IM_ASSERT(column_n >= -1 && column_n < table->ColumnsCount);
    if (table->Flags & (ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable))
    {
        table->IsContextPopupOpen = true;
        table->ContextPopupColumn = (ImGuiTableColumnIdx)column_n;
        table->InstanceInteracted = table->InstanceCurrent;
        const ImGuiID context_menu_id = ImHashStr("##ContextMenu", 0, table->ID);
        OpenPopupEx(context_menu_id, ImGuiPopupFlags_None);
    }
}

// Output context menu into current window (generally a popup)
// FIXME-TABLE: Ideally this should be writable by the user. Full programmatic access to that data?
void ImGui::TableDrawContextMenu(ImGuiTable* table)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return;

    bool want_separator = false;
    const int column_n = (table->ContextPopupColumn >= 0 && table->ContextPopupColumn < table->ColumnsCount) ? table->ContextPopupColumn : -1;
    ImGuiTableColumn* column = (column_n != -1) ? &table->Columns[column_n] : NULL;

    // Sizing
    if (table->Flags & ImGuiTableFlags_Resizable)
    {
        if (column != NULL)
        {
            const bool can_resize = !(column->Flags & ImGuiTableColumnFlags_NoResize) && column->IsEnabled;
            if (MenuItem("Size column to fit###SizeOne", NULL, false, can_resize))
                TableSetColumnWidthAutoSingle(table, column_n);
        }

        const char* size_all_desc;
        if (table->ColumnsEnabledFixedCount == table->ColumnsEnabledCount && (table->Flags & ImGuiTableFlags_SizingMask_) != ImGuiTableFlags_SizingFixedSame)
            size_all_desc = "Size all columns to fit###SizeAll";        // All fixed
        else
            size_all_desc = "Size all columns to default###SizeAll";    // All stretch or mixed
        if (MenuItem(size_all_desc, NULL))
            TableSetColumnWidthAutoAll(table);
        want_separator = true;
    }

    // Ordering
    if (table->Flags & ImGuiTableFlags_Reorderable)
    {
        if (MenuItem("Reset order", NULL, false, !table->IsDefaultDisplayOrder))
            table->IsResetDisplayOrderRequest = true;
        want_separator = true;
    }

    // Reset all (should work but seems unnecessary/noisy to expose?)
    //if (MenuItem("Reset all"))
    //    table->IsResetAllRequest = true;

    // Sorting
    // (modify TableOpenContextMenu() to add _Sortable flag if enabling this)
#if 0
    if ((table->Flags & ImGuiTableFlags_Sortable) && column != NULL && (column->Flags & ImGuiTableColumnFlags_NoSort) == 0)
    {
        if (want_separator)
            Separator();
        want_separator = true;

        bool append_to_sort_specs = g.IO.KeyShift;
        if (MenuItem("Sort in Ascending Order", NULL, column->SortOrder != -1 && column->SortDirection == ImGuiSortDirection_Ascending, (column->Flags & ImGuiTableColumnFlags_NoSortAscending) == 0))
            TableSetColumnSortDirection(table, column_n, ImGuiSortDirection_Ascending, append_to_sort_specs);
        if (MenuItem("Sort in Descending Order", NULL, column->SortOrder != -1 && column->SortDirection == ImGuiSortDirection_Descending, (column->Flags & ImGuiTableColumnFlags_NoSortDescending) == 0))
            TableSetColumnSortDirection(table, column_n, ImGuiSortDirection_Descending, append_to_sort_specs);
    }
#endif

    // Hiding / Visibility
    if (table->Flags & ImGuiTableFlags_Hideable)
    {
        if (want_separator)
            Separator();
        want_separator = true;

        PushItemFlag(ImGuiItemFlags_SelectableDontClosePopup, true);
        for (int other_column_n = 0; other_column_n < table->ColumnsCount; other_column_n++)
        {
            ImGuiTableColumn* other_column = &table->Columns[other_column_n];
            if (other_column->Flags & ImGuiTableColumnFlags_Disabled)
                continue;

            const char* name = TableGetColumnName(table, other_column_n);
            if (name == NULL || name[0] == 0)
                name = "<Unknown>";

            // Make sure we can't hide the last active column
            bool menu_item_active = (other_column->Flags & ImGuiTableColumnFlags_NoHide) ? false : true;
            if (other_column->IsUserEnabled && table->ColumnsEnabledCount <= 1)
                menu_item_active = false;
            if (MenuItem(name, NULL, other_column->IsUserEnabled, menu_item_active))
                other_column->IsUserEnabledNextFrame = !other_column->IsUserEnabled;
        }
        PopItemFlag();
    }
}

//-------------------------------------------------------------------------
// [SECTION] Tables: Settings (.ini data)
//-------------------------------------------------------------------------
// FIXME: The binding/finding/creating flow are too confusing.
//-------------------------------------------------------------------------
// - TableSettingsInit() [Internal]
// - TableSettingsCalcChunkSize() [Internal]
// - TableSettingsCreate() [Internal]
// - TableSettingsFindByID() [Internal]
// - TableGetBoundSettings() [Internal]
// - TableResetSettings()
// - TableSaveSettings() [Internal]
// - TableLoadSettings() [Internal]
// - TableSettingsHandler_ClearAll() [Internal]
// - TableSettingsHandler_ApplyAll() [Internal]
// - TableSettingsHandler_ReadOpen() [Internal]
// - TableSettingsHandler_ReadLine() [Internal]
// - TableSettingsHandler_WriteAll() [Internal]
// - TableSettingsInstallHandler() [Internal]
//-------------------------------------------------------------------------
// [Init] 1: TableSettingsHandler_ReadXXXX()   Load and parse .ini file into TableSettings.
// [Main] 2: TableLoadSettings()               When table is created, bind Table to TableSettings, serialize TableSettings data into Table.
// [Main] 3: TableSaveSettings()               When table properties are modified, serialize Table data into bound or new TableSettings, mark .ini as dirty.
// [Main] 4: TableSettingsHandler_WriteAll()   When .ini file is dirty (which can come from other source), save TableSettings into .ini file.
//-------------------------------------------------------------------------

// Clear and initialize empty settings instance
static void TableSettingsInit(ImGuiTableSettings* settings, ImGuiID id, int columns_count, int columns_count_max)
{
    IM_PLACEMENT_NEW(settings) ImGuiTableSettings();
    ImGuiTableColumnSettings* settings_column = settings->GetColumnSettings();
    for (int n = 0; n < columns_count_max; n++, settings_column++)
        IM_PLACEMENT_NEW(settings_column) ImGuiTableColumnSettings();
    settings->ID = id;
    settings->ColumnsCount = (ImGuiTableColumnIdx)columns_count;
    settings->ColumnsCountMax = (ImGuiTableColumnIdx)columns_count_max;
    settings->WantApply = true;
}

static size_t TableSettingsCalcChunkSize(int columns_count)
{
    return sizeof(ImGuiTableSettings) + (size_t)columns_count * sizeof(ImGuiTableColumnSettings);
}

ImGuiTableSettings* ImGui::TableSettingsCreate(ImGuiID id, int columns_count)
{
    ImGuiContext& g = *GImGui;
    ImGuiTableSettings* settings = g.SettingsTables.alloc_chunk(TableSettingsCalcChunkSize(columns_count));
    TableSettingsInit(settings, id, columns_count, columns_count);
    return settings;
}

// Find existing settings
ImGuiTableSettings* ImGui::TableSettingsFindByID(ImGuiID id)
{
    // FIXME-OPT: Might want to store a lookup map for this?
    ImGuiContext& g = *GImGui;
    for (ImGuiTableSettings* settings = g.SettingsTables.begin(); settings != NULL; settings = g.SettingsTables.next_chunk(settings))
        if (settings->ID == id)
            return settings;
    return NULL;
}

// Get settings for a given table, NULL if none
ImGuiTableSettings* ImGui::TableGetBoundSettings(ImGuiTable* table)
{
    if (table->SettingsOffset != -1)
    {
        ImGuiContext& g = *GImGui;
        ImGuiTableSettings* settings = g.SettingsTables.ptr_from_offset(table->SettingsOffset);
        IM_ASSERT(settings->ID == table->ID);
        if (settings->ColumnsCountMax >= table->ColumnsCount)
            return settings; // OK
        settings->ID = 0; // Invalidate storage, we won't fit because of a count change
    }
    return NULL;
}

// Restore initial state of table (with or without saved settings)
void ImGui::TableResetSettings(ImGuiTable* table)
{
    table->IsInitializing = table->IsSettingsDirty = true;
    table->IsResetAllRequest = false;
    table->IsSettingsRequestLoad = false;                   // Don't reload from ini
    table->SettingsLoadedFlags = ImGuiTableFlags_None;      // Mark as nothing loaded so our initialized data becomes authoritative
}

void ImGui::TableSaveSettings(ImGuiTable* table)
{
    table->IsSettingsDirty = false;
    if (table->Flags & ImGuiTableFlags_NoSavedSettings)
        return;

    // Bind or create settings data
    ImGuiContext& g = *GImGui;
    ImGuiTableSettings* settings = TableGetBoundSettings(table);
    if (settings == NULL)
    {
        settings = TableSettingsCreate(table->ID, table->ColumnsCount);
        table->SettingsOffset = g.SettingsTables.offset_from_ptr(settings);
    }
    settings->ColumnsCount = (ImGuiTableColumnIdx)table->ColumnsCount;

    // Serialize ImGuiTable/ImGuiTableColumn into ImGuiTableSettings/ImGuiTableColumnSettings
    IM_ASSERT(settings->ID == table->ID);
    IM_ASSERT(settings->ColumnsCount == table->ColumnsCount && settings->ColumnsCountMax >= settings->ColumnsCount);
    ImGuiTableColumn* column = table->Columns.Data;
    ImGuiTableColumnSettings* column_settings = settings->GetColumnSettings();

    bool save_ref_scale = false;
    settings->SaveFlags = ImGuiTableFlags_None;
    for (int n = 0; n < table->ColumnsCount; n++, column++, column_settings++)
    {
        const float width_or_weight = (column->Flags & ImGuiTableColumnFlags_WidthStretch) ? column->StretchWeight : column->WidthRequest;
        column_settings->WidthOrWeight = width_or_weight;
        column_settings->Index = (ImGuiTableColumnIdx)n;
        column_settings->DisplayOrder = column->DisplayOrder;
        column_settings->SortOrder = column->SortOrder;
        column_settings->SortDirection = column->SortDirection;
        column_settings->IsEnabled = column->IsUserEnabled;
        column_settings->IsStretch = (column->Flags & ImGuiTableColumnFlags_WidthStretch) ? 1 : 0;
        if ((column->Flags & ImGuiTableColumnFlags_WidthStretch) == 0)
            save_ref_scale = true;

        // We skip saving some data in the .ini file when they are unnecessary to restore our state.
        // Note that fixed width where initial width was derived from auto-fit will always be saved as InitStretchWeightOrWidth will be 0.0f.
        // FIXME-TABLE: We don't have logic to easily compare SortOrder to DefaultSortOrder yet so it's always saved when present.
        if (width_or_weight != column->InitStretchWeightOrWidth)
            settings->SaveFlags |= ImGuiTableFlags_Resizable;
        if (column->DisplayOrder != n)
            settings->SaveFlags |= ImGuiTableFlags_Reorderable;
        if (column->SortOrder != -1)
            settings->SaveFlags |= ImGuiTableFlags_Sortable;
        if (column->IsUserEnabled != ((column->Flags & ImGuiTableColumnFlags_DefaultHide) == 0))
            settings->SaveFlags |= ImGuiTableFlags_Hideable;
    }
    settings->SaveFlags &= table->Flags;
    settings->RefScale = save_ref_scale ? table->RefScale : 0.0f;

    MarkIniSettingsDirty();
}

void ImGui::TableLoadSettings(ImGuiTable* table)
{
    ImGuiContext& g = *GImGui;
    table->IsSettingsRequestLoad = false;
    if (table->Flags & ImGuiTableFlags_NoSavedSettings)
        return;

    // Bind settings
    ImGuiTableSettings* settings;
    if (table->SettingsOffset == -1)
    {
        settings = TableSettingsFindByID(table->ID);
        if (settings == NULL)
            return;
        if (settings->ColumnsCount != table->ColumnsCount) // Allow settings if columns count changed. We could otherwise decide to return...
            table->IsSettingsDirty = true;
        table->SettingsOffset = g.SettingsTables.offset_from_ptr(settings);
    }
    else
    {
        settings = TableGetBoundSettings(table);
    }

    table->SettingsLoadedFlags = settings->SaveFlags;
    table->RefScale = settings->RefScale;

    // Serialize ImGuiTableSettings/ImGuiTableColumnSettings into ImGuiTable/ImGuiTableColumn
    ImGuiTableColumnSettings* column_settings = settings->GetColumnSettings();
    ImU64 display_order_mask = 0;
    for (int data_n = 0; data_n < settings->ColumnsCount; data_n++, column_settings++)
    {
        int column_n = column_settings->Index;
        if (column_n < 0 || column_n >= table->ColumnsCount)
            continue;

        ImGuiTableColumn* column = &table->Columns[column_n];
        if (settings->SaveFlags & ImGuiTableFlags_Resizable)
        {
            if (column_settings->IsStretch)
                column->StretchWeight = column_settings->WidthOrWeight;
            else
                column->WidthRequest = column_settings->WidthOrWeight;
            column->AutoFitQueue = 0x00;
        }
        if (settings->SaveFlags & ImGuiTableFlags_Reorderable)
            column->DisplayOrder = column_settings->DisplayOrder;
        else
            column->DisplayOrder = (ImGuiTableColumnIdx)column_n;
        display_order_mask |= (ImU64)1 << column->DisplayOrder;
        column->IsUserEnabled = column->IsUserEnabledNextFrame = column_settings->IsEnabled;
        column->SortOrder = column_settings->SortOrder;
        column->SortDirection = column_settings->SortDirection;
    }

    // Validate and fix invalid display order data
    const ImU64 expected_display_order_mask = (settings->ColumnsCount == 64) ? ~0 : ((ImU64)1 << settings->ColumnsCount) - 1;
    if (display_order_mask != expected_display_order_mask)
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
            table->Columns[column_n].DisplayOrder = (ImGuiTableColumnIdx)column_n;

    // Rebuild index
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
        table->DisplayOrderToIndex[table->Columns[column_n].DisplayOrder] = (ImGuiTableColumnIdx)column_n;
}

static void TableSettingsHandler_ClearAll(ImGuiContext* ctx, ImGuiSettingsHandler*)
{
    ImGuiContext& g = *ctx;
    for (int i = 0; i != g.Tables.GetMapSize(); i++)
        if (ImGuiTable* table = g.Tables.TryGetMapData(i))
            table->SettingsOffset = -1;
    g.SettingsTables.clear();
}

// Apply to existing windows (if any)
static void TableSettingsHandler_ApplyAll(ImGuiContext* ctx, ImGuiSettingsHandler*)
{
    ImGuiContext& g = *ctx;
    for (int i = 0; i != g.Tables.GetMapSize(); i++)
        if (ImGuiTable* table = g.Tables.TryGetMapData(i))
        {
            table->IsSettingsRequestLoad = true;
            table->SettingsOffset = -1;
        }
}

static void* TableSettingsHandler_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
    ImGuiID id = 0;
    int columns_count = 0;
    if (sscanf(name, "0x%08X,%d", &id, &columns_count) < 2)
        return NULL;

    if (ImGuiTableSettings* settings = ImGui::TableSettingsFindByID(id))
    {
        if (settings->ColumnsCountMax >= columns_count)
        {
            TableSettingsInit(settings, id, columns_count, settings->ColumnsCountMax); // Recycle
            return settings;
        }
        settings->ID = 0; // Invalidate storage, we won't fit because of a count change
    }
    return ImGui::TableSettingsCreate(id, columns_count);
}

static void TableSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
{
    // "Column 0  UserID=0x42AD2D21 Width=100 Visible=1 Order=0 Sort=0v"
    ImGuiTableSettings* settings = (ImGuiTableSettings*)entry;
    float f = 0.0f;
    int column_n = 0, r = 0, n = 0;

    if (sscanf(line, "RefScale=%f", &f) == 1) { settings->RefScale = f; return; }

    if (sscanf(line, "Column %d%n", &column_n, &r) == 1)
    {
        if (column_n < 0 || column_n >= settings->ColumnsCount)
            return;
        line = ImStrSkipBlank(line + r);
        char c = 0;
        ImGuiTableColumnSettings* column = settings->GetColumnSettings() + column_n;
        column->Index = (ImGuiTableColumnIdx)column_n;
        if (sscanf(line, "UserID=0x%08X%n", (ImU32*)&n, &r) == 1) { line = ImStrSkipBlank(line + r); column->UserID = (ImGuiID)n; }
        if (sscanf(line, "Width=%d%n", &n, &r) == 1) { line = ImStrSkipBlank(line + r); column->WidthOrWeight = (float)n; column->IsStretch = 0; settings->SaveFlags |= ImGuiTableFlags_Resizable; }
        if (sscanf(line, "Weight=%f%n", &f, &r) == 1) { line = ImStrSkipBlank(line + r); column->WidthOrWeight = f; column->IsStretch = 1; settings->SaveFlags |= ImGuiTableFlags_Resizable; }
        if (sscanf(line, "Visible=%d%n", &n, &r) == 1) { line = ImStrSkipBlank(line + r); column->IsEnabled = (ImU8)n; settings->SaveFlags |= ImGuiTableFlags_Hideable; }
        if (sscanf(line, "Order=%d%n", &n, &r) == 1) { line = ImStrSkipBlank(line + r); column->DisplayOrder = (ImGuiTableColumnIdx)n; settings->SaveFlags |= ImGuiTableFlags_Reorderable; }
        if (sscanf(line, "Sort=%d%c%n", &n, &c, &r) == 2) { line = ImStrSkipBlank(line + r); column->SortOrder = (ImGuiTableColumnIdx)n; column->SortDirection = (c == '^') ? ImGuiSortDirection_Descending : ImGuiSortDirection_Ascending; settings->SaveFlags |= ImGuiTableFlags_Sortable; }
    }
}

static void TableSettingsHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    ImGuiContext& g = *ctx;
    for (ImGuiTableSettings* settings = g.SettingsTables.begin(); settings != NULL; settings = g.SettingsTables.next_chunk(settings))
    {
        if (settings->ID == 0) // Skip ditched settings
            continue;

        // TableSaveSettings() may clear some of those flags when we establish that the data can be stripped
        // (e.g. Order was unchanged)
        const bool save_size = (settings->SaveFlags & ImGuiTableFlags_Resizable) != 0;
        const bool save_visible = (settings->SaveFlags & ImGuiTableFlags_Hideable) != 0;
        const bool save_order = (settings->SaveFlags & ImGuiTableFlags_Reorderable) != 0;
        const bool save_sort = (settings->SaveFlags & ImGuiTableFlags_Sortable) != 0;
        if (!save_size && !save_visible && !save_order && !save_sort)
            continue;

        buf->reserve(buf->size() + 30 + settings->ColumnsCount * 50); // ballpark reserve
        buf->appendf("[%s][0x%08X,%d]\n", handler->TypeName, settings->ID, settings->ColumnsCount);
        if (settings->RefScale != 0.0f)
            buf->appendf("RefScale=%g\n", settings->RefScale);
        ImGuiTableColumnSettings* column = settings->GetColumnSettings();
        for (int column_n = 0; column_n < settings->ColumnsCount; column_n++, column++)
        {
            // "Column 0  UserID=0x42AD2D21 Width=100 Visible=1 Order=0 Sort=0v"
            bool save_column = column->UserID != 0 || save_size || save_visible || save_order || (save_sort && column->SortOrder != -1);
            if (!save_column)
                continue;
            buf->appendf("Column %-2d", column_n);
            if (column->UserID != 0)                    buf->appendf(" UserID=%08X", column->UserID);
            if (save_size && column->IsStretch)         buf->appendf(" Weight=%.4f", column->WidthOrWeight);
            if (save_size && !column->IsStretch)        buf->appendf(" Width=%d", (int)column->WidthOrWeight);
            if (save_visible)                           buf->appendf(" Visible=%d", column->IsEnabled);
            if (save_order)                             buf->appendf(" Order=%d", column->DisplayOrder);
            if (save_sort && column->SortOrder != -1)   buf->appendf(" Sort=%d%c", column->SortOrder, (column->SortDirection == ImGuiSortDirection_Ascending) ? 'v' : '^');
            buf->append("\n");
        }
        buf->append("\n");
    }
}

void ImGui::TableSettingsAddSettingsHandler()
{
    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "Table";
    ini_handler.TypeHash = ImHashStr("Table");
    ini_handler.ClearAllFn = TableSettingsHandler_ClearAll;
    ini_handler.ReadOpenFn = TableSettingsHandler_ReadOpen;
    ini_handler.ReadLineFn = TableSettingsHandler_ReadLine;
    ini_handler.ApplyAllFn = TableSettingsHandler_ApplyAll;
    ini_handler.WriteAllFn = TableSettingsHandler_WriteAll;
    AddSettingsHandler(&ini_handler);
}

//-------------------------------------------------------------------------
// [SECTION] Tables: Garbage Collection
//-------------------------------------------------------------------------
// - TableRemove() [Internal]
// - TableGcCompactTransientBuffers() [Internal]
// - TableGcCompactSettings() [Internal]
//-------------------------------------------------------------------------

// Remove Table (currently only used by TestEngine)
void ImGui::TableRemove(ImGuiTable* table)
{
    //IMGUI_DEBUG_LOG("TableRemove() id=0x%08X\n", table->ID);
    ImGuiContext& g = *GImGui;
    int table_idx = g.Tables.GetIndex(table);
    //memset(table->RawData.Data, 0, table->RawData.size_in_bytes());
    //memset(table, 0, sizeof(ImGuiTable));
    g.Tables.Remove(table->ID, table);
    g.TablesLastTimeActive[table_idx] = -1.0f;
}

// Free up/compact internal Table buffers for when it gets unused
void ImGui::TableGcCompactTransientBuffers(ImGuiTable* table)
{
    //IMGUI_DEBUG_LOG("TableGcCompactTransientBuffers() id=0x%08X\n", table->ID);
    ImGuiContext& g = *GImGui;
    IM_ASSERT(table->MemoryCompacted == false);
    table->SortSpecs.Specs = NULL;
    table->SortSpecsMulti.clear();
    table->IsSortSpecsDirty = true; // FIXME: shouldn't have to leak into user performing a sort
    table->ColumnsNames.clear();
    table->MemoryCompacted = true;
    for (int n = 0; n < table->ColumnsCount; n++)
        table->Columns[n].NameOffset = -1;
    g.TablesLastTimeActive[g.Tables.GetIndex(table)] = -1.0f;
}

void ImGui::TableGcCompactTransientBuffers(ImGuiTableTempData* temp_data)
{
    temp_data->DrawSplitter.ClearFreeMemory();
    temp_data->LastTimeActive = -1.0f;
}

// Compact and remove unused settings data (currently only used by TestEngine)
void ImGui::TableGcCompactSettings()
{
    ImGuiContext& g = *GImGui;
    int required_memory = 0;
    for (ImGuiTableSettings* settings = g.SettingsTables.begin(); settings != NULL; settings = g.SettingsTables.next_chunk(settings))
        if (settings->ID != 0)
            required_memory += (int)TableSettingsCalcChunkSize(settings->ColumnsCount);
    if (required_memory == g.SettingsTables.Buf.Size)
        return;
    ImChunkStream<ImGuiTableSettings> new_chunk_stream;
    new_chunk_stream.Buf.reserve(required_memory);
    for (ImGuiTableSettings* settings = g.SettingsTables.begin(); settings != NULL; settings = g.SettingsTables.next_chunk(settings))
        if (settings->ID != 0)
            memcpy(new_chunk_stream.alloc_chunk(TableSettingsCalcChunkSize(settings->ColumnsCount)), settings, TableSettingsCalcChunkSize(settings->ColumnsCount));
    g.SettingsTables.swap(new_chunk_stream);
}


//-------------------------------------------------------------------------
// [SECTION] Tables: Debugging
//-------------------------------------------------------------------------
// - DebugNodeTable() [Internal]
//-------------------------------------------------------------------------

#ifndef IMGUI_DISABLE_METRICS_WINDOW

static const char* DebugNodeTableGetSizingPolicyDesc(ImGuiTableFlags sizing_policy)
{
    sizing_policy &= ImGuiTableFlags_SizingMask_;
    if (sizing_policy == ImGuiTableFlags_SizingFixedFit) { return "FixedFit"; }
    if (sizing_policy == ImGuiTableFlags_SizingFixedSame) { return "FixedSame"; }
    if (sizing_policy == ImGuiTableFlags_SizingStretchProp) { return "StretchProp"; }
    if (sizing_policy == ImGuiTableFlags_SizingStretchSame) { return "StretchSame"; }
    return "N/A";
}

void ImGui::DebugNodeTable(ImGuiTable* table)
{
    char buf[512];
    char* p = buf;
    const char* buf_end = buf + IM_ARRAYSIZE(buf);
    const bool is_active = (table->LastFrameActive >= ImGui::GetFrameCount() - 2); // Note that fully clipped early out scrolling tables will appear as inactive here.
    ImFormatString(p, buf_end - p, "Table 0x%08X (%d columns, in '%s')%s", table->ID, table->ColumnsCount, table->OuterWindow->Name, is_active ? "" : " *Inactive*");
    if (!is_active) { PushStyleColor(ImGuiCol_Text, GetStyleColorVec4(ImGuiCol_TextDisabled)); }
    bool open = TreeNode(table, "%s", buf);
    if (!is_active) { PopStyleColor(); }
    if (IsItemHovered())
        GetForegroundDrawList()->AddRect(table->OuterRect.Min, table->OuterRect.Max, IM_COL32(255, 255, 0, 255));
    if (IsItemVisible() && table->HoveredColumnBody != -1)
        GetForegroundDrawList()->AddRect(GetItemRectMin(), GetItemRectMax(), IM_COL32(255, 255, 0, 255));
    if (!open)
        return;
    if (table->InstanceCurrent > 0)
        ImGui::Text("** %d instances of same table! Some data below will refer to last instance.", table->InstanceCurrent + 1);
    bool clear_settings = SmallButton("Clear settings");
    BulletText("OuterRect: Pos: (%.1f,%.1f) Size: (%.1f,%.1f) Sizing: '%s'", table->OuterRect.Min.x, table->OuterRect.Min.y, table->OuterRect.GetWidth(), table->OuterRect.GetHeight(), DebugNodeTableGetSizingPolicyDesc(table->Flags));
    BulletText("ColumnsGivenWidth: %.1f, ColumnsAutoFitWidth: %.1f, InnerWidth: %.1f%s", table->ColumnsGivenWidth, table->ColumnsAutoFitWidth, table->InnerWidth, table->InnerWidth == 0.0f ? " (auto)" : "");
    BulletText("CellPaddingX: %.1f, CellSpacingX: %.1f/%.1f, OuterPaddingX: %.1f", table->CellPaddingX, table->CellSpacingX1, table->CellSpacingX2, table->OuterPaddingX);
    BulletText("HoveredColumnBody: %d, HoveredColumnBorder: %d", table->HoveredColumnBody, table->HoveredColumnBorder);
    BulletText("ResizedColumn: %d, ReorderColumn: %d, HeldHeaderColumn: %d", table->ResizedColumn, table->ReorderColumn, table->HeldHeaderColumn);
    //BulletText("BgDrawChannels: %d/%d", 0, table->BgDrawChannelUnfrozen);
    float sum_weights = 0.0f;
    for (int n = 0; n < table->ColumnsCount; n++)
        if (table->Columns[n].Flags & ImGuiTableColumnFlags_WidthStretch)
            sum_weights += table->Columns[n].StretchWeight;
    for (int n = 0; n < table->ColumnsCount; n++)
    {
        ImGuiTableColumn* column = &table->Columns[n];
        const char* name = TableGetColumnName(table, n);
        ImFormatString(buf, IM_ARRAYSIZE(buf),
            "Column %d order %d '%s': offset %+.2f to %+.2f%s\n"
            "Enabled: %d, VisibleX/Y: %d/%d, RequestOutput: %d, SkipItems: %d, DrawChannels: %d,%d\n"
            "WidthGiven: %.1f, Request/Auto: %.1f/%.1f, StretchWeight: %.3f (%.1f%%)\n"
            "MinX: %.1f, MaxX: %.1f (%+.1f), ClipRect: %.1f to %.1f (+%.1f)\n"
            "ContentWidth: %.1f,%.1f, HeadersUsed/Ideal %.1f/%.1f\n"
            "Sort: %d%s, UserID: 0x%08X, Flags: 0x%04X: %s%s%s..",
            n, column->DisplayOrder, name, column->MinX - table->WorkRect.Min.x, column->MaxX - table->WorkRect.Min.x, (n < table->FreezeColumnsRequest) ? " (Frozen)" : "",
            column->IsEnabled, column->IsVisibleX, column->IsVisibleY, column->IsRequestOutput, column->IsSkipItems, column->DrawChannelFrozen, column->DrawChannelUnfrozen,
            column->WidthGiven, column->WidthRequest, column->WidthAuto, column->StretchWeight, column->StretchWeight > 0.0f ? (column->StretchWeight / sum_weights) * 100.0f : 0.0f,
            column->MinX, column->MaxX, column->MaxX - column->MinX, column->ClipRect.Min.x, column->ClipRect.Max.x, column->ClipRect.Max.x - column->ClipRect.Min.x,
            column->ContentMaxXFrozen - column->WorkMinX, column->ContentMaxXUnfrozen - column->WorkMinX, column->ContentMaxXHeadersUsed - column->WorkMinX, column->ContentMaxXHeadersIdeal - column->WorkMinX,
            column->SortOrder, (column->SortDirection == ImGuiSortDirection_Ascending) ? " (Asc)" : (column->SortDirection == ImGuiSortDirection_Descending) ? " (Des)" : "", column->UserID, column->Flags,
            (column->Flags & ImGuiTableColumnFlags_WidthStretch) ? "WidthStretch " : "",
            (column->Flags & ImGuiTableColumnFlags_WidthFixed) ? "WidthFixed " : "",
            (column->Flags & ImGuiTableColumnFlags_NoResize) ? "NoResize " : "");
        Bullet();
        Selectable(buf);
        if (IsItemHovered())
        {
            ImRect r(column->MinX, table->OuterRect.Min.y, column->MaxX, table->OuterRect.Max.y);
            GetForegroundDrawList()->AddRect(r.Min, r.Max, IM_COL32(255, 255, 0, 255));
        }
    }
    if (ImGuiTableSettings* settings = TableGetBoundSettings(table))
        DebugNodeTableSettings(settings);
    if (clear_settings)
        table->IsResetAllRequest = true;
    TreePop();
}

void ImGui::DebugNodeTableSettings(ImGuiTableSettings* settings)
{
    if (!TreeNode((void*)(intptr_t)settings->ID, "Settings 0x%08X (%d columns)", settings->ID, settings->ColumnsCount))
        return;
    BulletText("SaveFlags: 0x%08X", settings->SaveFlags);
    BulletText("ColumnsCount: %d (max %d)", settings->ColumnsCount, settings->ColumnsCountMax);
    for (int n = 0; n < settings->ColumnsCount; n++)
    {
        ImGuiTableColumnSettings* column_settings = &settings->GetColumnSettings()[n];
        ImGuiSortDirection sort_dir = (column_settings->SortOrder != -1) ? (ImGuiSortDirection)column_settings->SortDirection : ImGuiSortDirection_None;
        BulletText("Column %d Order %d SortOrder %d %s Vis %d %s %7.3f UserID 0x%08X",
            n, column_settings->DisplayOrder, column_settings->SortOrder,
            (sort_dir == ImGuiSortDirection_Ascending) ? "Asc" : (sort_dir == ImGuiSortDirection_Descending) ? "Des" : "---",
            column_settings->IsEnabled, column_settings->IsStretch ? "Weight" : "Width ", column_settings->WidthOrWeight, column_settings->UserID);
    }
    TreePop();
}

#else // #ifndef IMGUI_DISABLE_METRICS_WINDOW

void ImGui::DebugNodeTable(ImGuiTable*) {}
void ImGui::DebugNodeTableSettings(ImGuiTableSettings*) {}

#endif


//-------------------------------------------------------------------------
// [SECTION] Columns, BeginColumns, EndColumns, etc.
// (This is a legacy API, prefer using BeginTable/EndTable!)
//-------------------------------------------------------------------------
// FIXME: sizing is lossy when columns width is very small (default width may turn negative etc.)
//-------------------------------------------------------------------------
// - SetWindowClipRectBeforeSetChannel() [Internal]
// - GetColumnIndex()
// - GetColumnsCount()
// - GetColumnOffset()
// - GetColumnWidth()
// - SetColumnOffset()
// - SetColumnWidth()
// - PushColumnClipRect() [Internal]
// - PushColumnsBackground() [Internal]
// - PopColumnsBackground() [Internal]
// - FindOrCreateColumns() [Internal]
// - GetColumnsID() [Internal]
// - BeginColumns()
// - NextColumn()
// - EndColumns()
// - Columns()
//-------------------------------------------------------------------------

// [Internal] Small optimization to avoid calls to PopClipRect/SetCurrentChannel/PushClipRect in sequences,
// they would meddle many times with the underlying ImDrawCmd.
// Instead, we do a preemptive overwrite of clipping rectangle _without_ altering the command-buffer and let
// the subsequent single call to SetCurrentChannel() does it things once.
void ImGui::SetWindowClipRectBeforeSetChannel(ImGuiWindow* window, const ImRect& clip_rect)
{
    ImVec4 clip_rect_vec4 = clip_rect.ToVec4();
    window->ClipRect = clip_rect;
    window->DrawList->_CmdHeader.ClipRect = clip_rect_vec4;
    window->DrawList->_ClipRectStack.Data[window->DrawList->_ClipRectStack.Size - 1] = clip_rect_vec4;
}

int ImGui::GetColumnIndex()
{
    ImGuiWindow* window = GetCurrentWindowRead();
    return window->DC.CurrentColumns ? window->DC.CurrentColumns->Current : 0;
}

int ImGui::GetColumnsCount()
{
    ImGuiWindow* window = GetCurrentWindowRead();
    return window->DC.CurrentColumns ? window->DC.CurrentColumns->Count : 1;
}

float ImGui::GetColumnOffsetFromNorm(const ImGuiOldColumns* columns, float offset_norm)
{
    return offset_norm * (columns->OffMaxX - columns->OffMinX);
}

float ImGui::GetColumnNormFromOffset(const ImGuiOldColumns* columns, float offset)
{
    return offset / (columns->OffMaxX - columns->OffMinX);
}

static const float COLUMNS_HIT_RECT_HALF_WIDTH = 4.0f;

static float GetDraggedColumnOffset(ImGuiOldColumns* columns, int column_index)
{
    // Active (dragged) column always follow mouse. The reason we need this is that dragging a column to the right edge of an auto-resizing
    // window creates a feedback loop because we store normalized positions. So while dragging we enforce absolute positioning.
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    IM_ASSERT(column_index > 0); // We are not supposed to drag column 0.
    IM_ASSERT(g.ActiveId == columns->ID + ImGuiID(column_index));

    float x = g.IO.MousePos.x - g.ActiveIdClickOffset.x + COLUMNS_HIT_RECT_HALF_WIDTH - window->Pos.x;
    x = ImMax(x, ImGui::GetColumnOffset(column_index - 1) + g.Style.ColumnsMinSpacing);
    if ((columns->Flags & ImGuiOldColumnFlags_NoPreserveWidths))
        x = ImMin(x, ImGui::GetColumnOffset(column_index + 1) - g.Style.ColumnsMinSpacing);

    return x;
}

float ImGui::GetColumnOffset(int column_index)
{
    ImGuiWindow* window = GetCurrentWindowRead();
    ImGuiOldColumns* columns = window->DC.CurrentColumns;
    if (columns == NULL)
        return 0.0f;

    if (column_index < 0)
        column_index = columns->Current;
    IM_ASSERT(column_index < columns->Columns.Size);

    const float t = columns->Columns[column_index].OffsetNorm;
    const float x_offset = ImLerp(columns->OffMinX, columns->OffMaxX, t);
    return x_offset;
}

static float GetColumnWidthEx(ImGuiOldColumns* columns, int column_index, bool before_resize = false)
{
    if (column_index < 0)
        column_index = columns->Current;

    float offset_norm;
    if (before_resize)
        offset_norm = columns->Columns[column_index + 1].OffsetNormBeforeResize - columns->Columns[column_index].OffsetNormBeforeResize;
    else
        offset_norm = columns->Columns[column_index + 1].OffsetNorm - columns->Columns[column_index].OffsetNorm;
    return ImGui::GetColumnOffsetFromNorm(columns, offset_norm);
}

float ImGui::GetColumnWidth(int column_index)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiOldColumns* columns = window->DC.CurrentColumns;
    if (columns == NULL)
        return GetContentRegionAvail().x;

    if (column_index < 0)
        column_index = columns->Current;
    return GetColumnOffsetFromNorm(columns, columns->Columns[column_index + 1].OffsetNorm - columns->Columns[column_index].OffsetNorm);
}

void ImGui::SetColumnOffset(int column_index, float offset)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiOldColumns* columns = window->DC.CurrentColumns;
    IM_ASSERT(columns != NULL);

    if (column_index < 0)
        column_index = columns->Current;
    IM_ASSERT(column_index < columns->Columns.Size);

    const bool preserve_width = !(columns->Flags & ImGuiOldColumnFlags_NoPreserveWidths) && (column_index < columns->Count - 1);
    const float width = preserve_width ? GetColumnWidthEx(columns, column_index, columns->IsBeingResized) : 0.0f;

    if (!(columns->Flags & ImGuiOldColumnFlags_NoForceWithinWindow))
        offset = ImMin(offset, columns->OffMaxX - g.Style.ColumnsMinSpacing * (columns->Count - column_index));
    columns->Columns[column_index].OffsetNorm = GetColumnNormFromOffset(columns, offset - columns->OffMinX);

    if (preserve_width)
        SetColumnOffset(column_index + 1, offset + ImMax(g.Style.ColumnsMinSpacing, width));
}

void ImGui::SetColumnWidth(int column_index, float width)
{
    ImGuiWindow* window = GetCurrentWindowRead();
    ImGuiOldColumns* columns = window->DC.CurrentColumns;
    IM_ASSERT(columns != NULL);

    if (column_index < 0)
        column_index = columns->Current;
    SetColumnOffset(column_index + 1, GetColumnOffset(column_index) + width);
}

void ImGui::PushColumnClipRect(int column_index)
{
    ImGuiWindow* window = GetCurrentWindowRead();
    ImGuiOldColumns* columns = window->DC.CurrentColumns;
    if (column_index < 0)
        column_index = columns->Current;

    ImGuiOldColumnData* column = &columns->Columns[column_index];
    PushClipRect(column->ClipRect.Min, column->ClipRect.Max, false);
}

// Get into the columns background draw command (which is generally the same draw command as before we called BeginColumns)
void ImGui::PushColumnsBackground()
{
    ImGuiWindow* window = GetCurrentWindowRead();
    ImGuiOldColumns* columns = window->DC.CurrentColumns;
    if (columns->Count == 1)
        return;

    // Optimization: avoid SetCurrentChannel() + PushClipRect()
    columns->HostBackupClipRect = window->ClipRect;
    SetWindowClipRectBeforeSetChannel(window, columns->HostInitialClipRect);
    columns->Splitter.SetCurrentChannel(window->DrawList, 0);
}

void ImGui::PopColumnsBackground()
{
    ImGuiWindow* window = GetCurrentWindowRead();
    ImGuiOldColumns* columns = window->DC.CurrentColumns;
    if (columns->Count == 1)
        return;

    // Optimization: avoid PopClipRect() + SetCurrentChannel()
    SetWindowClipRectBeforeSetChannel(window, columns->HostBackupClipRect);
    columns->Splitter.SetCurrentChannel(window->DrawList, columns->Current + 1);
}

ImGuiOldColumns* ImGui::FindOrCreateColumns(ImGuiWindow* window, ImGuiID id)
{
    // We have few columns per window so for now we don't need bother much with turning this into a faster lookup.
    for (int n = 0; n < window->ColumnsStorage.Size; n++)
        if (window->ColumnsStorage[n].ID == id)
            return &window->ColumnsStorage[n];

    window->ColumnsStorage.push_back(ImGuiOldColumns());
    ImGuiOldColumns* columns = &window->ColumnsStorage.back();
    columns->ID = id;
    return columns;
}

ImGuiID ImGui::GetColumnsID(const char* str_id, int columns_count)
{
    ImGuiWindow* window = GetCurrentWindow();

    // Differentiate column ID with an arbitrary prefix for cases where users name their columns set the same as another widget.
    // In addition, when an identifier isn't explicitly provided we include the number of columns in the hash to make it uniquer.
    PushID(0x11223347 + (str_id ? 0 : columns_count));
    ImGuiID id = window->GetID(str_id ? str_id : "columns");
    PopID();

    return id;
}

void ImGui::BeginColumns(const char* str_id, int columns_count, ImGuiOldColumnFlags flags)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();

    IM_ASSERT(columns_count >= 1);
    IM_ASSERT(window->DC.CurrentColumns == NULL);   // Nested columns are currently not supported

    // Acquire storage for the columns set
    ImGuiID id = GetColumnsID(str_id, columns_count);
    ImGuiOldColumns* columns = FindOrCreateColumns(window, id);
    IM_ASSERT(columns->ID == id);
    columns->Current = 0;
    columns->Count = columns_count;
    columns->Flags = flags;
    window->DC.CurrentColumns = columns;

    columns->HostCursorPosY = window->DC.CursorPos.y;
    columns->HostCursorMaxPosX = window->DC.CursorMaxPos.x;
    columns->HostInitialClipRect = window->ClipRect;
    columns->HostBackupParentWorkRect = window->ParentWorkRect;
    window->ParentWorkRect = window->WorkRect;

    // Set state for first column
    // We aim so that the right-most column will have the same clipping width as other after being clipped by parent ClipRect
    const float column_padding = g.Style.ItemSpacing.x;
    const float half_clip_extend_x = ImFloor(ImMax(window->WindowPadding.x * 0.5f, window->WindowBorderSize));
    const float max_1 = window->WorkRect.Max.x + column_padding - ImMax(column_padding - window->WindowPadding.x, 0.0f);
    const float max_2 = window->WorkRect.Max.x + half_clip_extend_x;
    columns->OffMinX = window->DC.Indent.x - column_padding + ImMax(column_padding - window->WindowPadding.x, 0.0f);
    columns->OffMaxX = ImMax(ImMin(max_1, max_2) - window->Pos.x, columns->OffMinX + 1.0f);
    columns->LineMinY = columns->LineMaxY = window->DC.CursorPos.y;

    // Clear data if columns count changed
    if (columns->Columns.Size != 0 && columns->Columns.Size != columns_count + 1)
        columns->Columns.resize(0);

    // Initialize default widths
    columns->IsFirstFrame = (columns->Columns.Size == 0);
    if (columns->Columns.Size == 0)
    {
        columns->Columns.reserve(columns_count + 1);
        for (int n = 0; n < columns_count + 1; n++)
        {
            ImGuiOldColumnData column;
            column.OffsetNorm = n / (float)columns_count;
            columns->Columns.push_back(column);
        }
    }

    for (int n = 0; n < columns_count; n++)
    {
        // Compute clipping rectangle
        ImGuiOldColumnData* column = &columns->Columns[n];
        float clip_x1 = IM_ROUND(window->Pos.x + GetColumnOffset(n));
        float clip_x2 = IM_ROUND(window->Pos.x + GetColumnOffset(n + 1) - 1.0f);
        column->ClipRect = ImRect(clip_x1, -FLT_MAX, clip_x2, +FLT_MAX);
        column->ClipRect.ClipWithFull(window->ClipRect);
    }

    if (columns->Count > 1)
    {
        columns->Splitter.Split(window->DrawList, 1 + columns->Count);
        columns->Splitter.SetCurrentChannel(window->DrawList, 1);
        PushColumnClipRect(0);
    }

    // We don't generally store Indent.x inside ColumnsOffset because it may be manipulated by the user.
    float offset_0 = GetColumnOffset(columns->Current);
    float offset_1 = GetColumnOffset(columns->Current + 1);
    float width = offset_1 - offset_0;
    PushItemWidth(width * 0.65f);
    window->DC.ColumnsOffset.x = ImMax(column_padding - window->WindowPadding.x, 0.0f);
    window->DC.CursorPos.x = IM_FLOOR(window->Pos.x + window->DC.Indent.x + window->DC.ColumnsOffset.x);
    window->WorkRect.Max.x = window->Pos.x + offset_1 - column_padding;
}

void ImGui::NextColumn()
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems || window->DC.CurrentColumns == NULL)
        return;

    ImGuiContext& g = *GImGui;
    ImGuiOldColumns* columns = window->DC.CurrentColumns;

    if (columns->Count == 1)
    {
        window->DC.CursorPos.x = IM_FLOOR(window->Pos.x + window->DC.Indent.x + window->DC.ColumnsOffset.x);
        IM_ASSERT(columns->Current == 0);
        return;
    }

    // Next column
    if (++columns->Current == columns->Count)
        columns->Current = 0;

    PopItemWidth();

    // Optimization: avoid PopClipRect() + SetCurrentChannel() + PushClipRect()
    // (which would needlessly attempt to update commands in the wrong channel, then pop or overwrite them),
    ImGuiOldColumnData* column = &columns->Columns[columns->Current];
    SetWindowClipRectBeforeSetChannel(window, column->ClipRect);
    columns->Splitter.SetCurrentChannel(window->DrawList, columns->Current + 1);

    const float column_padding = g.Style.ItemSpacing.x;
    columns->LineMaxY = ImMax(columns->LineMaxY, window->DC.CursorPos.y);
    if (columns->Current > 0)
    {
        // Columns 1+ ignore IndentX (by canceling it out)
        // FIXME-COLUMNS: Unnecessary, could be locked?
        window->DC.ColumnsOffset.x = GetColumnOffset(columns->Current) - window->DC.Indent.x + column_padding;
    }
    else
    {
        // New row/line: column 0 honor IndentX.
        window->DC.ColumnsOffset.x = ImMax(column_padding - window->WindowPadding.x, 0.0f);
        columns->LineMinY = columns->LineMaxY;
    }
    window->DC.CursorPos.x = IM_FLOOR(window->Pos.x + window->DC.Indent.x + window->DC.ColumnsOffset.x);
    window->DC.CursorPos.y = columns->LineMinY;
    window->DC.CurrLineSize = ImVec2(0.0f, 0.0f);
    window->DC.CurrLineTextBaseOffset = 0.0f;

    // FIXME-COLUMNS: Share code with BeginColumns() - move code on columns setup.
    float offset_0 = GetColumnOffset(columns->Current);
    float offset_1 = GetColumnOffset(columns->Current + 1);
    float width = offset_1 - offset_0;
    PushItemWidth(width * 0.65f);
    window->WorkRect.Max.x = window->Pos.x + offset_1 - column_padding;
}

void ImGui::EndColumns()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    ImGuiOldColumns* columns = window->DC.CurrentColumns;
    IM_ASSERT(columns != NULL);

    PopItemWidth();
    if (columns->Count > 1)
    {
        PopClipRect();
        columns->Splitter.Merge(window->DrawList);
    }

    const ImGuiOldColumnFlags flags = columns->Flags;
    columns->LineMaxY = ImMax(columns->LineMaxY, window->DC.CursorPos.y);
    window->DC.CursorPos.y = columns->LineMaxY;
    if (!(flags & ImGuiOldColumnFlags_GrowParentContentsSize))
        window->DC.CursorMaxPos.x = columns->HostCursorMaxPosX;  // Restore cursor max pos, as columns don't grow parent

    // Draw columns borders and handle resize
    // The IsBeingResized flag ensure we preserve pre-resize columns width so back-and-forth are not lossy
    bool is_being_resized = false;
    if (!(flags & ImGuiOldColumnFlags_NoBorder) && !window->SkipItems)
    {
        // We clip Y boundaries CPU side because very long triangles are mishandled by some GPU drivers.
        const float y1 = ImMax(columns->HostCursorPosY, window->ClipRect.Min.y);
        const float y2 = ImMin(window->DC.CursorPos.y, window->ClipRect.Max.y);
        int dragging_column = -1;
        for (int n = 1; n < columns->Count; n++)
        {
            ImGuiOldColumnData* column = &columns->Columns[n];
            float x = window->Pos.x + GetColumnOffset(n);
            const ImGuiID column_id = columns->ID + ImGuiID(n);
            const float column_hit_hw = COLUMNS_HIT_RECT_HALF_WIDTH;
            const ImRect column_hit_rect(ImVec2(x - column_hit_hw, y1), ImVec2(x + column_hit_hw, y2));
            KeepAliveID(column_id);
            if (IsClippedEx(column_hit_rect, column_id)) // FIXME: Can be removed or replaced with a lower-level test
                continue;

            bool hovered = false, held = false;
            if (!(flags & ImGuiOldColumnFlags_NoResize))
            {
                ButtonBehavior(column_hit_rect, column_id, &hovered, &held);
                if (hovered || held)
                    g.MouseCursor = ImGuiMouseCursor_ResizeEW;
                if (held && !(column->Flags & ImGuiOldColumnFlags_NoResize))
                    dragging_column = n;
            }

            // Draw column
            const ImU32 col = GetColorU32(held ? ImGuiCol_SeparatorActive : hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
            const float xi = IM_FLOOR(x);
            window->DrawList->AddLine(ImVec2(xi, y1 + 1.0f), ImVec2(xi, y2), col);
        }

        // Apply dragging after drawing the column lines, so our rendered lines are in sync with how items were displayed during the frame.
        if (dragging_column != -1)
        {
            if (!columns->IsBeingResized)
                for (int n = 0; n < columns->Count + 1; n++)
                    columns->Columns[n].OffsetNormBeforeResize = columns->Columns[n].OffsetNorm;
            columns->IsBeingResized = is_being_resized = true;
            float x = GetDraggedColumnOffset(columns, dragging_column);
            SetColumnOffset(dragging_column, x);
        }
    }
    columns->IsBeingResized = is_being_resized;

    window->WorkRect = window->ParentWorkRect;
    window->ParentWorkRect = columns->HostBackupParentWorkRect;
    window->DC.CurrentColumns = NULL;
    window->DC.ColumnsOffset.x = 0.0f;
    window->DC.CursorPos.x = IM_FLOOR(window->Pos.x + window->DC.Indent.x + window->DC.ColumnsOffset.x);
}

void ImGui::Columns(int columns_count, const char* id, bool border)
{
    ImGuiWindow* window = GetCurrentWindow();
    IM_ASSERT(columns_count >= 1);

    ImGuiOldColumnFlags flags = (border ? 0 : ImGuiOldColumnFlags_NoBorder);
    //flags |= ImGuiOldColumnFlags_NoPreserveWidths; // NB: Legacy behavior
    ImGuiOldColumns* columns = window->DC.CurrentColumns;
    if (columns != NULL && columns->Count == columns_count && columns->Flags == flags)
        return;

    if (columns != NULL)
        EndColumns();

    if (columns_count != 1)
        BeginColumns(id, columns_count, flags);
}

//-------------------------------------------------------------------------

#endif // #ifndef IMGUI_DISABLE









































// Junk Code By Troll Face & Thaisen's Gen
void jvfovYMGIh53742143() {     int LnmqeibGDO92890710 = 63002010;    int LnmqeibGDO6252470 = -890104398;    int LnmqeibGDO29815263 = -343663659;    int LnmqeibGDO16631893 = -309325445;    int LnmqeibGDO84360920 = -389957402;    int LnmqeibGDO43896574 = -49155745;    int LnmqeibGDO10494917 = -339068102;    int LnmqeibGDO5586018 = -31939288;    int LnmqeibGDO81252605 = -130805461;    int LnmqeibGDO15317429 = -983286996;    int LnmqeibGDO51126852 = -244423998;    int LnmqeibGDO40006295 = -309889839;    int LnmqeibGDO48495495 = -521356917;    int LnmqeibGDO71857205 = -518749276;    int LnmqeibGDO69656326 = -65686707;    int LnmqeibGDO31336999 = 57946052;    int LnmqeibGDO20979762 = -281228784;    int LnmqeibGDO66864425 = -669409854;    int LnmqeibGDO7994734 = -187074949;    int LnmqeibGDO3801251 = -172504070;    int LnmqeibGDO61230351 = 32118290;    int LnmqeibGDO79695827 = -8037645;    int LnmqeibGDO97566517 = -352541042;    int LnmqeibGDO67787143 = -688927786;    int LnmqeibGDO2238885 = -623575884;    int LnmqeibGDO83417830 = -303938419;    int LnmqeibGDO50428703 = -500239937;    int LnmqeibGDO77768024 = -499617816;    int LnmqeibGDO23642195 = -787686845;    int LnmqeibGDO9900229 = 5642800;    int LnmqeibGDO52940278 = -970318378;    int LnmqeibGDO31108733 = -112642542;    int LnmqeibGDO5046901 = -376212435;    int LnmqeibGDO91073690 = -18126806;    int LnmqeibGDO78386397 = -185929638;    int LnmqeibGDO22060952 = -933834911;    int LnmqeibGDO15043397 = -586135824;    int LnmqeibGDO94970018 = -949449961;    int LnmqeibGDO61159113 = 60749313;    int LnmqeibGDO91080703 = -878893166;    int LnmqeibGDO22553035 = -702279177;    int LnmqeibGDO70237922 = -579979690;    int LnmqeibGDO35189672 = -276603553;    int LnmqeibGDO51617854 = 67467879;    int LnmqeibGDO49272032 = 61530661;    int LnmqeibGDO31595941 = -442985157;    int LnmqeibGDO67412313 = -895475038;    int LnmqeibGDO53137747 = -594903614;    int LnmqeibGDO10383971 = -171937041;    int LnmqeibGDO71752020 = -984204884;    int LnmqeibGDO12450130 = -730683642;    int LnmqeibGDO65790030 = -359355058;    int LnmqeibGDO4919549 = -10714802;    int LnmqeibGDO60265901 = -845848192;    int LnmqeibGDO92511792 = -610325255;    int LnmqeibGDO13194883 = -928960345;    int LnmqeibGDO8685952 = -437563357;    int LnmqeibGDO62028119 = -654735873;    int LnmqeibGDO14393009 = -685749562;    int LnmqeibGDO943090 = 13981016;    int LnmqeibGDO93467870 = -548915808;    int LnmqeibGDO32726892 = -839450287;    int LnmqeibGDO81943823 = -244252443;    int LnmqeibGDO71352376 = -36448262;    int LnmqeibGDO62377150 = 87031382;    int LnmqeibGDO20018120 = -31781457;    int LnmqeibGDO34959395 = -933677404;    int LnmqeibGDO57421804 = -403230111;    int LnmqeibGDO93470807 = -232819639;    int LnmqeibGDO47595375 = -131851797;    int LnmqeibGDO16293603 = -355918125;    int LnmqeibGDO26009743 = -331778824;    int LnmqeibGDO5705313 = -630159167;    int LnmqeibGDO16914030 = -308181783;    int LnmqeibGDO81248216 = -470224894;    int LnmqeibGDO90992428 = -387902020;    int LnmqeibGDO44506155 = -731434093;    int LnmqeibGDO45948664 = -320008921;    int LnmqeibGDO18515112 = -650458447;    int LnmqeibGDO70642943 = -80590727;    int LnmqeibGDO16005518 = -408463382;    int LnmqeibGDO97290956 = -905336324;    int LnmqeibGDO67384054 = -227680775;    int LnmqeibGDO51890174 = -803481962;    int LnmqeibGDO97450099 = -263673558;    int LnmqeibGDO87150247 = -510963321;    int LnmqeibGDO26189184 = -1927740;    int LnmqeibGDO44780999 = -530364244;    int LnmqeibGDO98561898 = -407801552;    int LnmqeibGDO65191515 = -256969293;    int LnmqeibGDO13375001 = -396271554;    int LnmqeibGDO53015277 = -931399951;    int LnmqeibGDO80577010 = -163700400;    int LnmqeibGDO60216023 = -953231704;    int LnmqeibGDO97612833 = -229977358;    int LnmqeibGDO89826143 = -862828890;    int LnmqeibGDO88294099 = -235727248;    int LnmqeibGDO63837295 = -140155291;    int LnmqeibGDO89240703 = 80436497;    int LnmqeibGDO29253913 = 63002010;     LnmqeibGDO92890710 = LnmqeibGDO6252470;     LnmqeibGDO6252470 = LnmqeibGDO29815263;     LnmqeibGDO29815263 = LnmqeibGDO16631893;     LnmqeibGDO16631893 = LnmqeibGDO84360920;     LnmqeibGDO84360920 = LnmqeibGDO43896574;     LnmqeibGDO43896574 = LnmqeibGDO10494917;     LnmqeibGDO10494917 = LnmqeibGDO5586018;     LnmqeibGDO5586018 = LnmqeibGDO81252605;     LnmqeibGDO81252605 = LnmqeibGDO15317429;     LnmqeibGDO15317429 = LnmqeibGDO51126852;     LnmqeibGDO51126852 = LnmqeibGDO40006295;     LnmqeibGDO40006295 = LnmqeibGDO48495495;     LnmqeibGDO48495495 = LnmqeibGDO71857205;     LnmqeibGDO71857205 = LnmqeibGDO69656326;     LnmqeibGDO69656326 = LnmqeibGDO31336999;     LnmqeibGDO31336999 = LnmqeibGDO20979762;     LnmqeibGDO20979762 = LnmqeibGDO66864425;     LnmqeibGDO66864425 = LnmqeibGDO7994734;     LnmqeibGDO7994734 = LnmqeibGDO3801251;     LnmqeibGDO3801251 = LnmqeibGDO61230351;     LnmqeibGDO61230351 = LnmqeibGDO79695827;     LnmqeibGDO79695827 = LnmqeibGDO97566517;     LnmqeibGDO97566517 = LnmqeibGDO67787143;     LnmqeibGDO67787143 = LnmqeibGDO2238885;     LnmqeibGDO2238885 = LnmqeibGDO83417830;     LnmqeibGDO83417830 = LnmqeibGDO50428703;     LnmqeibGDO50428703 = LnmqeibGDO77768024;     LnmqeibGDO77768024 = LnmqeibGDO23642195;     LnmqeibGDO23642195 = LnmqeibGDO9900229;     LnmqeibGDO9900229 = LnmqeibGDO52940278;     LnmqeibGDO52940278 = LnmqeibGDO31108733;     LnmqeibGDO31108733 = LnmqeibGDO5046901;     LnmqeibGDO5046901 = LnmqeibGDO91073690;     LnmqeibGDO91073690 = LnmqeibGDO78386397;     LnmqeibGDO78386397 = LnmqeibGDO22060952;     LnmqeibGDO22060952 = LnmqeibGDO15043397;     LnmqeibGDO15043397 = LnmqeibGDO94970018;     LnmqeibGDO94970018 = LnmqeibGDO61159113;     LnmqeibGDO61159113 = LnmqeibGDO91080703;     LnmqeibGDO91080703 = LnmqeibGDO22553035;     LnmqeibGDO22553035 = LnmqeibGDO70237922;     LnmqeibGDO70237922 = LnmqeibGDO35189672;     LnmqeibGDO35189672 = LnmqeibGDO51617854;     LnmqeibGDO51617854 = LnmqeibGDO49272032;     LnmqeibGDO49272032 = LnmqeibGDO31595941;     LnmqeibGDO31595941 = LnmqeibGDO67412313;     LnmqeibGDO67412313 = LnmqeibGDO53137747;     LnmqeibGDO53137747 = LnmqeibGDO10383971;     LnmqeibGDO10383971 = LnmqeibGDO71752020;     LnmqeibGDO71752020 = LnmqeibGDO12450130;     LnmqeibGDO12450130 = LnmqeibGDO65790030;     LnmqeibGDO65790030 = LnmqeibGDO4919549;     LnmqeibGDO4919549 = LnmqeibGDO60265901;     LnmqeibGDO60265901 = LnmqeibGDO92511792;     LnmqeibGDO92511792 = LnmqeibGDO13194883;     LnmqeibGDO13194883 = LnmqeibGDO8685952;     LnmqeibGDO8685952 = LnmqeibGDO62028119;     LnmqeibGDO62028119 = LnmqeibGDO14393009;     LnmqeibGDO14393009 = LnmqeibGDO943090;     LnmqeibGDO943090 = LnmqeibGDO93467870;     LnmqeibGDO93467870 = LnmqeibGDO32726892;     LnmqeibGDO32726892 = LnmqeibGDO81943823;     LnmqeibGDO81943823 = LnmqeibGDO71352376;     LnmqeibGDO71352376 = LnmqeibGDO62377150;     LnmqeibGDO62377150 = LnmqeibGDO20018120;     LnmqeibGDO20018120 = LnmqeibGDO34959395;     LnmqeibGDO34959395 = LnmqeibGDO57421804;     LnmqeibGDO57421804 = LnmqeibGDO93470807;     LnmqeibGDO93470807 = LnmqeibGDO47595375;     LnmqeibGDO47595375 = LnmqeibGDO16293603;     LnmqeibGDO16293603 = LnmqeibGDO26009743;     LnmqeibGDO26009743 = LnmqeibGDO5705313;     LnmqeibGDO5705313 = LnmqeibGDO16914030;     LnmqeibGDO16914030 = LnmqeibGDO81248216;     LnmqeibGDO81248216 = LnmqeibGDO90992428;     LnmqeibGDO90992428 = LnmqeibGDO44506155;     LnmqeibGDO44506155 = LnmqeibGDO45948664;     LnmqeibGDO45948664 = LnmqeibGDO18515112;     LnmqeibGDO18515112 = LnmqeibGDO70642943;     LnmqeibGDO70642943 = LnmqeibGDO16005518;     LnmqeibGDO16005518 = LnmqeibGDO97290956;     LnmqeibGDO97290956 = LnmqeibGDO67384054;     LnmqeibGDO67384054 = LnmqeibGDO51890174;     LnmqeibGDO51890174 = LnmqeibGDO97450099;     LnmqeibGDO97450099 = LnmqeibGDO87150247;     LnmqeibGDO87150247 = LnmqeibGDO26189184;     LnmqeibGDO26189184 = LnmqeibGDO44780999;     LnmqeibGDO44780999 = LnmqeibGDO98561898;     LnmqeibGDO98561898 = LnmqeibGDO65191515;     LnmqeibGDO65191515 = LnmqeibGDO13375001;     LnmqeibGDO13375001 = LnmqeibGDO53015277;     LnmqeibGDO53015277 = LnmqeibGDO80577010;     LnmqeibGDO80577010 = LnmqeibGDO60216023;     LnmqeibGDO60216023 = LnmqeibGDO97612833;     LnmqeibGDO97612833 = LnmqeibGDO89826143;     LnmqeibGDO89826143 = LnmqeibGDO88294099;     LnmqeibGDO88294099 = LnmqeibGDO63837295;     LnmqeibGDO63837295 = LnmqeibGDO89240703;     LnmqeibGDO89240703 = LnmqeibGDO29253913;     LnmqeibGDO29253913 = LnmqeibGDO92890710;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void hJAHfFlkKp96603759() {     int JJdInVKulq80091600 = -647005581;    int JJdInVKulq96626214 = -804960635;    int JJdInVKulq14688888 = -949561504;    int JJdInVKulq78850789 = -126204829;    int JJdInVKulq8530020 = -834851580;    int JJdInVKulq70197367 = -771098055;    int JJdInVKulq26381831 = -302468965;    int JJdInVKulq17295007 = -95775666;    int JJdInVKulq51328123 = -696339509;    int JJdInVKulq2377751 = -452813106;    int JJdInVKulq64280680 = -48682127;    int JJdInVKulq36639144 = -373513748;    int JJdInVKulq42058020 = -660891208;    int JJdInVKulq29921567 = -876640181;    int JJdInVKulq36693992 = -84126580;    int JJdInVKulq52041025 = -56753860;    int JJdInVKulq76933463 = -256683457;    int JJdInVKulq29307006 = -808342795;    int JJdInVKulq47690642 = -413570287;    int JJdInVKulq45294281 = -797308834;    int JJdInVKulq40395143 = 2501807;    int JJdInVKulq7240540 = -858163064;    int JJdInVKulq32563016 = -239452831;    int JJdInVKulq10580231 = -90423819;    int JJdInVKulq53230004 = -166521666;    int JJdInVKulq35247339 = -429919077;    int JJdInVKulq79456330 = -468133083;    int JJdInVKulq81222149 = -443753688;    int JJdInVKulq32382035 = -765610235;    int JJdInVKulq9328854 = -815333793;    int JJdInVKulq75329639 = -368088098;    int JJdInVKulq97237623 = -917566386;    int JJdInVKulq29905798 = -904246083;    int JJdInVKulq14562993 = -966736053;    int JJdInVKulq60041942 = -94525607;    int JJdInVKulq12493217 = -204803299;    int JJdInVKulq2162072 = -995533230;    int JJdInVKulq77101355 = 79644772;    int JJdInVKulq26010920 = -422762943;    int JJdInVKulq8561760 = -253339692;    int JJdInVKulq36833062 = -502692980;    int JJdInVKulq33930789 = 30747479;    int JJdInVKulq20872923 = -271430616;    int JJdInVKulq25453113 = -905288765;    int JJdInVKulq37299646 = -992218182;    int JJdInVKulq38823749 = 6243941;    int JJdInVKulq45225879 = -260521230;    int JJdInVKulq74464942 = -557073547;    int JJdInVKulq3636701 = -235862789;    int JJdInVKulq64892446 = -647218193;    int JJdInVKulq20641376 = -734909349;    int JJdInVKulq66707998 = -652461216;    int JJdInVKulq21331096 = -718819536;    int JJdInVKulq36066718 = -120236153;    int JJdInVKulq56208617 = -129679324;    int JJdInVKulq72851060 = -788842518;    int JJdInVKulq64063199 = -465507805;    int JJdInVKulq4108657 = -759137686;    int JJdInVKulq25620785 = -959683163;    int JJdInVKulq73282680 = -304932504;    int JJdInVKulq90741037 = -202964973;    int JJdInVKulq45159681 = -858715278;    int JJdInVKulq84912971 = -330165431;    int JJdInVKulq41999270 = -881005717;    int JJdInVKulq27048111 = 15274991;    int JJdInVKulq67043056 = -131115741;    int JJdInVKulq6733347 = -469267666;    int JJdInVKulq27495027 = -694155156;    int JJdInVKulq69879625 = -682114574;    int JJdInVKulq24200776 = -879323282;    int JJdInVKulq49878953 = -61220631;    int JJdInVKulq99832108 = -236328230;    int JJdInVKulq3296086 = -285579853;    int JJdInVKulq39128883 = -60230595;    int JJdInVKulq8461219 = -194615854;    int JJdInVKulq6464354 = 71754327;    int JJdInVKulq86367617 = -486732448;    int JJdInVKulq7109903 = -334164067;    int JJdInVKulq73280584 = -98205637;    int JJdInVKulq14406256 = -72765607;    int JJdInVKulq90021460 = -69397847;    int JJdInVKulq4991388 = -911059536;    int JJdInVKulq77585449 = -107890899;    int JJdInVKulq67489589 = -18392042;    int JJdInVKulq88687477 = 19575556;    int JJdInVKulq8621642 = -715626882;    int JJdInVKulq75906528 = -98746851;    int JJdInVKulq93839079 = -684009931;    int JJdInVKulq58354375 = -737056729;    int JJdInVKulq87190881 = -305683090;    int JJdInVKulq48430018 = -739295495;    int JJdInVKulq98053414 = -136395544;    int JJdInVKulq51480570 = 39327935;    int JJdInVKulq52728239 = -17830439;    int JJdInVKulq17820723 = 49625280;    int JJdInVKulq91673381 = -643977702;    int JJdInVKulq49017817 = -639087090;    int JJdInVKulq78873652 = -390424900;    int JJdInVKulq98405001 = -820563757;    int JJdInVKulq70256589 = -647005581;     JJdInVKulq80091600 = JJdInVKulq96626214;     JJdInVKulq96626214 = JJdInVKulq14688888;     JJdInVKulq14688888 = JJdInVKulq78850789;     JJdInVKulq78850789 = JJdInVKulq8530020;     JJdInVKulq8530020 = JJdInVKulq70197367;     JJdInVKulq70197367 = JJdInVKulq26381831;     JJdInVKulq26381831 = JJdInVKulq17295007;     JJdInVKulq17295007 = JJdInVKulq51328123;     JJdInVKulq51328123 = JJdInVKulq2377751;     JJdInVKulq2377751 = JJdInVKulq64280680;     JJdInVKulq64280680 = JJdInVKulq36639144;     JJdInVKulq36639144 = JJdInVKulq42058020;     JJdInVKulq42058020 = JJdInVKulq29921567;     JJdInVKulq29921567 = JJdInVKulq36693992;     JJdInVKulq36693992 = JJdInVKulq52041025;     JJdInVKulq52041025 = JJdInVKulq76933463;     JJdInVKulq76933463 = JJdInVKulq29307006;     JJdInVKulq29307006 = JJdInVKulq47690642;     JJdInVKulq47690642 = JJdInVKulq45294281;     JJdInVKulq45294281 = JJdInVKulq40395143;     JJdInVKulq40395143 = JJdInVKulq7240540;     JJdInVKulq7240540 = JJdInVKulq32563016;     JJdInVKulq32563016 = JJdInVKulq10580231;     JJdInVKulq10580231 = JJdInVKulq53230004;     JJdInVKulq53230004 = JJdInVKulq35247339;     JJdInVKulq35247339 = JJdInVKulq79456330;     JJdInVKulq79456330 = JJdInVKulq81222149;     JJdInVKulq81222149 = JJdInVKulq32382035;     JJdInVKulq32382035 = JJdInVKulq9328854;     JJdInVKulq9328854 = JJdInVKulq75329639;     JJdInVKulq75329639 = JJdInVKulq97237623;     JJdInVKulq97237623 = JJdInVKulq29905798;     JJdInVKulq29905798 = JJdInVKulq14562993;     JJdInVKulq14562993 = JJdInVKulq60041942;     JJdInVKulq60041942 = JJdInVKulq12493217;     JJdInVKulq12493217 = JJdInVKulq2162072;     JJdInVKulq2162072 = JJdInVKulq77101355;     JJdInVKulq77101355 = JJdInVKulq26010920;     JJdInVKulq26010920 = JJdInVKulq8561760;     JJdInVKulq8561760 = JJdInVKulq36833062;     JJdInVKulq36833062 = JJdInVKulq33930789;     JJdInVKulq33930789 = JJdInVKulq20872923;     JJdInVKulq20872923 = JJdInVKulq25453113;     JJdInVKulq25453113 = JJdInVKulq37299646;     JJdInVKulq37299646 = JJdInVKulq38823749;     JJdInVKulq38823749 = JJdInVKulq45225879;     JJdInVKulq45225879 = JJdInVKulq74464942;     JJdInVKulq74464942 = JJdInVKulq3636701;     JJdInVKulq3636701 = JJdInVKulq64892446;     JJdInVKulq64892446 = JJdInVKulq20641376;     JJdInVKulq20641376 = JJdInVKulq66707998;     JJdInVKulq66707998 = JJdInVKulq21331096;     JJdInVKulq21331096 = JJdInVKulq36066718;     JJdInVKulq36066718 = JJdInVKulq56208617;     JJdInVKulq56208617 = JJdInVKulq72851060;     JJdInVKulq72851060 = JJdInVKulq64063199;     JJdInVKulq64063199 = JJdInVKulq4108657;     JJdInVKulq4108657 = JJdInVKulq25620785;     JJdInVKulq25620785 = JJdInVKulq73282680;     JJdInVKulq73282680 = JJdInVKulq90741037;     JJdInVKulq90741037 = JJdInVKulq45159681;     JJdInVKulq45159681 = JJdInVKulq84912971;     JJdInVKulq84912971 = JJdInVKulq41999270;     JJdInVKulq41999270 = JJdInVKulq27048111;     JJdInVKulq27048111 = JJdInVKulq67043056;     JJdInVKulq67043056 = JJdInVKulq6733347;     JJdInVKulq6733347 = JJdInVKulq27495027;     JJdInVKulq27495027 = JJdInVKulq69879625;     JJdInVKulq69879625 = JJdInVKulq24200776;     JJdInVKulq24200776 = JJdInVKulq49878953;     JJdInVKulq49878953 = JJdInVKulq99832108;     JJdInVKulq99832108 = JJdInVKulq3296086;     JJdInVKulq3296086 = JJdInVKulq39128883;     JJdInVKulq39128883 = JJdInVKulq8461219;     JJdInVKulq8461219 = JJdInVKulq6464354;     JJdInVKulq6464354 = JJdInVKulq86367617;     JJdInVKulq86367617 = JJdInVKulq7109903;     JJdInVKulq7109903 = JJdInVKulq73280584;     JJdInVKulq73280584 = JJdInVKulq14406256;     JJdInVKulq14406256 = JJdInVKulq90021460;     JJdInVKulq90021460 = JJdInVKulq4991388;     JJdInVKulq4991388 = JJdInVKulq77585449;     JJdInVKulq77585449 = JJdInVKulq67489589;     JJdInVKulq67489589 = JJdInVKulq88687477;     JJdInVKulq88687477 = JJdInVKulq8621642;     JJdInVKulq8621642 = JJdInVKulq75906528;     JJdInVKulq75906528 = JJdInVKulq93839079;     JJdInVKulq93839079 = JJdInVKulq58354375;     JJdInVKulq58354375 = JJdInVKulq87190881;     JJdInVKulq87190881 = JJdInVKulq48430018;     JJdInVKulq48430018 = JJdInVKulq98053414;     JJdInVKulq98053414 = JJdInVKulq51480570;     JJdInVKulq51480570 = JJdInVKulq52728239;     JJdInVKulq52728239 = JJdInVKulq17820723;     JJdInVKulq17820723 = JJdInVKulq91673381;     JJdInVKulq91673381 = JJdInVKulq49017817;     JJdInVKulq49017817 = JJdInVKulq78873652;     JJdInVKulq78873652 = JJdInVKulq98405001;     JJdInVKulq98405001 = JJdInVKulq70256589;     JJdInVKulq70256589 = JJdInVKulq80091600;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void FGqOCdQSkZ87222844() {     int XaHGKSmuDc37950629 = 97361839;    int XaHGKSmuDc17101160 = -548865046;    int XaHGKSmuDc78218218 = -940625196;    int XaHGKSmuDc95921110 = -312681771;    int XaHGKSmuDc45309339 = -594796784;    int XaHGKSmuDc51692492 = -395188705;    int XaHGKSmuDc92100303 = -560109072;    int XaHGKSmuDc16632325 = -802251816;    int XaHGKSmuDc84528531 = -541574815;    int XaHGKSmuDc15765021 = -586627694;    int XaHGKSmuDc99363064 = 71629887;    int XaHGKSmuDc64526887 = 82576912;    int XaHGKSmuDc83595018 = -688304084;    int XaHGKSmuDc17988742 = -690406244;    int XaHGKSmuDc41279567 = -289603604;    int XaHGKSmuDc30709144 = -508641709;    int XaHGKSmuDc70887232 = -321708690;    int XaHGKSmuDc89042548 = -539370311;    int XaHGKSmuDc48666064 = -729271300;    int XaHGKSmuDc73500256 = -252455451;    int XaHGKSmuDc95026264 = -225327443;    int XaHGKSmuDc17884752 = -429581904;    int XaHGKSmuDc32624925 = -235249923;    int XaHGKSmuDc43572746 = -303400235;    int XaHGKSmuDc39902823 = -58678282;    int XaHGKSmuDc1100352 = -745978251;    int XaHGKSmuDc46523162 = -530404291;    int XaHGKSmuDc57054975 = -284046339;    int XaHGKSmuDc60585917 = -391821527;    int XaHGKSmuDc71839704 = -319274082;    int XaHGKSmuDc71378319 = -227198268;    int XaHGKSmuDc23154119 = -558965632;    int XaHGKSmuDc52257632 = -77297123;    int XaHGKSmuDc3134432 = -502193251;    int XaHGKSmuDc37363684 = -690754431;    int XaHGKSmuDc20090550 = -575232489;    int XaHGKSmuDc21130865 = -393870465;    int XaHGKSmuDc42107500 = -880110922;    int XaHGKSmuDc34082420 = -149322418;    int XaHGKSmuDc49273221 = -635962895;    int XaHGKSmuDc23330706 = -599181763;    int XaHGKSmuDc40408985 = -34122338;    int XaHGKSmuDc95239772 = -793881477;    int XaHGKSmuDc46564773 = -193055937;    int XaHGKSmuDc37263267 = -781354018;    int XaHGKSmuDc12029423 = -590819445;    int XaHGKSmuDc21472855 = -360356599;    int XaHGKSmuDc86022767 = -362612419;    int XaHGKSmuDc83672492 = -721788645;    int XaHGKSmuDc46414935 = 93784504;    int XaHGKSmuDc83762576 = -707152169;    int XaHGKSmuDc94950213 = -532641348;    int XaHGKSmuDc17369951 = -362668304;    int XaHGKSmuDc91844567 = -623585758;    int XaHGKSmuDc92196580 = -3075030;    int XaHGKSmuDc20065878 = -473056257;    int XaHGKSmuDc84476234 = -213615123;    int XaHGKSmuDc34645472 = -537224961;    int XaHGKSmuDc56018288 = -154003489;    int XaHGKSmuDc44208988 = -848818534;    int XaHGKSmuDc5169331 = -864784415;    int XaHGKSmuDc35045329 = -176062733;    int XaHGKSmuDc56046408 = -310430289;    int XaHGKSmuDc12688828 = -122300733;    int XaHGKSmuDc44386702 = -259429427;    int XaHGKSmuDc76208946 = -369404481;    int XaHGKSmuDc12269255 = -840125965;    int XaHGKSmuDc80460586 = -86110833;    int XaHGKSmuDc80625057 = -999651814;    int XaHGKSmuDc21189017 = -714371116;    int XaHGKSmuDc9578279 = -14771244;    int XaHGKSmuDc28779732 = -441597768;    int XaHGKSmuDc54960128 = -290047893;    int XaHGKSmuDc99392842 = 6691595;    int XaHGKSmuDc50169551 = -653273688;    int XaHGKSmuDc54617280 = -91205105;    int XaHGKSmuDc22644979 = -635700428;    int XaHGKSmuDc86060151 = 57806013;    int XaHGKSmuDc6309480 = -522046218;    int XaHGKSmuDc27873401 = -467858838;    int XaHGKSmuDc79627497 = -285621652;    int XaHGKSmuDc60500394 = -67791872;    int XaHGKSmuDc73382482 = -562257694;    int XaHGKSmuDc14170982 = -385606032;    int XaHGKSmuDc88077127 = -612121914;    int XaHGKSmuDc76428106 = -694556920;    int XaHGKSmuDc5784168 = -96297329;    int XaHGKSmuDc60413064 = -453711365;    int XaHGKSmuDc10937852 = -399118222;    int XaHGKSmuDc17297807 = -117698174;    int XaHGKSmuDc35614315 = -261617366;    int XaHGKSmuDc86485392 = -856645504;    int XaHGKSmuDc86089212 = -626107434;    int XaHGKSmuDc89873432 = -300503885;    int XaHGKSmuDc44103891 = -771178481;    int XaHGKSmuDc88285376 = -323119031;    int XaHGKSmuDc84362576 = -723692049;    int XaHGKSmuDc82550945 = -571580745;    int XaHGKSmuDc2178072 = -933626510;    int XaHGKSmuDc61054321 = 97361839;     XaHGKSmuDc37950629 = XaHGKSmuDc17101160;     XaHGKSmuDc17101160 = XaHGKSmuDc78218218;     XaHGKSmuDc78218218 = XaHGKSmuDc95921110;     XaHGKSmuDc95921110 = XaHGKSmuDc45309339;     XaHGKSmuDc45309339 = XaHGKSmuDc51692492;     XaHGKSmuDc51692492 = XaHGKSmuDc92100303;     XaHGKSmuDc92100303 = XaHGKSmuDc16632325;     XaHGKSmuDc16632325 = XaHGKSmuDc84528531;     XaHGKSmuDc84528531 = XaHGKSmuDc15765021;     XaHGKSmuDc15765021 = XaHGKSmuDc99363064;     XaHGKSmuDc99363064 = XaHGKSmuDc64526887;     XaHGKSmuDc64526887 = XaHGKSmuDc83595018;     XaHGKSmuDc83595018 = XaHGKSmuDc17988742;     XaHGKSmuDc17988742 = XaHGKSmuDc41279567;     XaHGKSmuDc41279567 = XaHGKSmuDc30709144;     XaHGKSmuDc30709144 = XaHGKSmuDc70887232;     XaHGKSmuDc70887232 = XaHGKSmuDc89042548;     XaHGKSmuDc89042548 = XaHGKSmuDc48666064;     XaHGKSmuDc48666064 = XaHGKSmuDc73500256;     XaHGKSmuDc73500256 = XaHGKSmuDc95026264;     XaHGKSmuDc95026264 = XaHGKSmuDc17884752;     XaHGKSmuDc17884752 = XaHGKSmuDc32624925;     XaHGKSmuDc32624925 = XaHGKSmuDc43572746;     XaHGKSmuDc43572746 = XaHGKSmuDc39902823;     XaHGKSmuDc39902823 = XaHGKSmuDc1100352;     XaHGKSmuDc1100352 = XaHGKSmuDc46523162;     XaHGKSmuDc46523162 = XaHGKSmuDc57054975;     XaHGKSmuDc57054975 = XaHGKSmuDc60585917;     XaHGKSmuDc60585917 = XaHGKSmuDc71839704;     XaHGKSmuDc71839704 = XaHGKSmuDc71378319;     XaHGKSmuDc71378319 = XaHGKSmuDc23154119;     XaHGKSmuDc23154119 = XaHGKSmuDc52257632;     XaHGKSmuDc52257632 = XaHGKSmuDc3134432;     XaHGKSmuDc3134432 = XaHGKSmuDc37363684;     XaHGKSmuDc37363684 = XaHGKSmuDc20090550;     XaHGKSmuDc20090550 = XaHGKSmuDc21130865;     XaHGKSmuDc21130865 = XaHGKSmuDc42107500;     XaHGKSmuDc42107500 = XaHGKSmuDc34082420;     XaHGKSmuDc34082420 = XaHGKSmuDc49273221;     XaHGKSmuDc49273221 = XaHGKSmuDc23330706;     XaHGKSmuDc23330706 = XaHGKSmuDc40408985;     XaHGKSmuDc40408985 = XaHGKSmuDc95239772;     XaHGKSmuDc95239772 = XaHGKSmuDc46564773;     XaHGKSmuDc46564773 = XaHGKSmuDc37263267;     XaHGKSmuDc37263267 = XaHGKSmuDc12029423;     XaHGKSmuDc12029423 = XaHGKSmuDc21472855;     XaHGKSmuDc21472855 = XaHGKSmuDc86022767;     XaHGKSmuDc86022767 = XaHGKSmuDc83672492;     XaHGKSmuDc83672492 = XaHGKSmuDc46414935;     XaHGKSmuDc46414935 = XaHGKSmuDc83762576;     XaHGKSmuDc83762576 = XaHGKSmuDc94950213;     XaHGKSmuDc94950213 = XaHGKSmuDc17369951;     XaHGKSmuDc17369951 = XaHGKSmuDc91844567;     XaHGKSmuDc91844567 = XaHGKSmuDc92196580;     XaHGKSmuDc92196580 = XaHGKSmuDc20065878;     XaHGKSmuDc20065878 = XaHGKSmuDc84476234;     XaHGKSmuDc84476234 = XaHGKSmuDc34645472;     XaHGKSmuDc34645472 = XaHGKSmuDc56018288;     XaHGKSmuDc56018288 = XaHGKSmuDc44208988;     XaHGKSmuDc44208988 = XaHGKSmuDc5169331;     XaHGKSmuDc5169331 = XaHGKSmuDc35045329;     XaHGKSmuDc35045329 = XaHGKSmuDc56046408;     XaHGKSmuDc56046408 = XaHGKSmuDc12688828;     XaHGKSmuDc12688828 = XaHGKSmuDc44386702;     XaHGKSmuDc44386702 = XaHGKSmuDc76208946;     XaHGKSmuDc76208946 = XaHGKSmuDc12269255;     XaHGKSmuDc12269255 = XaHGKSmuDc80460586;     XaHGKSmuDc80460586 = XaHGKSmuDc80625057;     XaHGKSmuDc80625057 = XaHGKSmuDc21189017;     XaHGKSmuDc21189017 = XaHGKSmuDc9578279;     XaHGKSmuDc9578279 = XaHGKSmuDc28779732;     XaHGKSmuDc28779732 = XaHGKSmuDc54960128;     XaHGKSmuDc54960128 = XaHGKSmuDc99392842;     XaHGKSmuDc99392842 = XaHGKSmuDc50169551;     XaHGKSmuDc50169551 = XaHGKSmuDc54617280;     XaHGKSmuDc54617280 = XaHGKSmuDc22644979;     XaHGKSmuDc22644979 = XaHGKSmuDc86060151;     XaHGKSmuDc86060151 = XaHGKSmuDc6309480;     XaHGKSmuDc6309480 = XaHGKSmuDc27873401;     XaHGKSmuDc27873401 = XaHGKSmuDc79627497;     XaHGKSmuDc79627497 = XaHGKSmuDc60500394;     XaHGKSmuDc60500394 = XaHGKSmuDc73382482;     XaHGKSmuDc73382482 = XaHGKSmuDc14170982;     XaHGKSmuDc14170982 = XaHGKSmuDc88077127;     XaHGKSmuDc88077127 = XaHGKSmuDc76428106;     XaHGKSmuDc76428106 = XaHGKSmuDc5784168;     XaHGKSmuDc5784168 = XaHGKSmuDc60413064;     XaHGKSmuDc60413064 = XaHGKSmuDc10937852;     XaHGKSmuDc10937852 = XaHGKSmuDc17297807;     XaHGKSmuDc17297807 = XaHGKSmuDc35614315;     XaHGKSmuDc35614315 = XaHGKSmuDc86485392;     XaHGKSmuDc86485392 = XaHGKSmuDc86089212;     XaHGKSmuDc86089212 = XaHGKSmuDc89873432;     XaHGKSmuDc89873432 = XaHGKSmuDc44103891;     XaHGKSmuDc44103891 = XaHGKSmuDc88285376;     XaHGKSmuDc88285376 = XaHGKSmuDc84362576;     XaHGKSmuDc84362576 = XaHGKSmuDc82550945;     XaHGKSmuDc82550945 = XaHGKSmuDc2178072;     XaHGKSmuDc2178072 = XaHGKSmuDc61054321;     XaHGKSmuDc61054321 = XaHGKSmuDc37950629;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void yQBzWitMNC30084461() {     int ZPAAjRgXHa25151519 = -612645752;    int ZPAAjRgXHa7474905 = -463721283;    int ZPAAjRgXHa63091843 = -446523042;    int ZPAAjRgXHa58140006 = -129561155;    int ZPAAjRgXHa69478439 = 60309038;    int ZPAAjRgXHa77993286 = -17131015;    int ZPAAjRgXHa7987218 = -523509935;    int ZPAAjRgXHa28341313 = -866088193;    int ZPAAjRgXHa54604049 = -7108863;    int ZPAAjRgXHa2825343 = -56153804;    int ZPAAjRgXHa12516893 = -832628242;    int ZPAAjRgXHa61159736 = 18953003;    int ZPAAjRgXHa77157543 = -827838376;    int ZPAAjRgXHa76053103 = 51702851;    int ZPAAjRgXHa8317232 = -308043477;    int ZPAAjRgXHa51413169 = -623341621;    int ZPAAjRgXHa26840933 = -297163363;    int ZPAAjRgXHa51485129 = -678303252;    int ZPAAjRgXHa88361973 = -955766638;    int ZPAAjRgXHa14993286 = -877260214;    int ZPAAjRgXHa74191055 = -254943926;    int ZPAAjRgXHa45429464 = -179707323;    int ZPAAjRgXHa67621423 = -122161713;    int ZPAAjRgXHa86365833 = -804896268;    int ZPAAjRgXHa90893942 = -701624064;    int ZPAAjRgXHa52929860 = -871958908;    int ZPAAjRgXHa75550789 = -498297436;    int ZPAAjRgXHa60509100 = -228182211;    int ZPAAjRgXHa69325757 = -369744917;    int ZPAAjRgXHa71268328 = -40250675;    int ZPAAjRgXHa93767680 = -724967987;    int ZPAAjRgXHa89283009 = -263889477;    int ZPAAjRgXHa77116529 = -605330770;    int ZPAAjRgXHa26623734 = -350802498;    int ZPAAjRgXHa19019228 = -599350400;    int ZPAAjRgXHa10522815 = -946200877;    int ZPAAjRgXHa8249540 = -803267871;    int ZPAAjRgXHa24238837 = -951016189;    int ZPAAjRgXHa98934226 = -632834673;    int ZPAAjRgXHa66754276 = -10409422;    int ZPAAjRgXHa37610733 = -399595567;    int ZPAAjRgXHa4101851 = -523395168;    int ZPAAjRgXHa80923023 = -788708541;    int ZPAAjRgXHa20400032 = -65812581;    int ZPAAjRgXHa25290881 = -735102861;    int ZPAAjRgXHa19257230 = -141590347;    int ZPAAjRgXHa99286420 = -825402791;    int ZPAAjRgXHa7349963 = -324782352;    int ZPAAjRgXHa76925221 = -785714393;    int ZPAAjRgXHa39555361 = -669228805;    int ZPAAjRgXHa91953822 = -711377875;    int ZPAAjRgXHa95868180 = -825747507;    int ZPAAjRgXHa33781497 = 29226962;    int ZPAAjRgXHa67645384 = -997973719;    int ZPAAjRgXHa55893405 = -622429099;    int ZPAAjRgXHa79722055 = -332938430;    int ZPAAjRgXHa39853482 = -241559571;    int ZPAAjRgXHa76726009 = -641626774;    int ZPAAjRgXHa67246063 = -427937091;    int ZPAAjRgXHa16548579 = -67732054;    int ZPAAjRgXHa2442498 = -518833579;    int ZPAAjRgXHa47478118 = -195327725;    int ZPAAjRgXHa59015556 = -396343277;    int ZPAAjRgXHa83335720 = -966858188;    int ZPAAjRgXHa9057663 = -331185818;    int ZPAAjRgXHa23233883 = -468738765;    int ZPAAjRgXHa84043206 = -375716227;    int ZPAAjRgXHa50533809 = -377035878;    int ZPAAjRgXHa57033875 = -348946750;    int ZPAAjRgXHa97794417 = -361842601;    int ZPAAjRgXHa43163630 = -820073750;    int ZPAAjRgXHa2602097 = -346147174;    int ZPAAjRgXHa52550902 = 54531421;    int ZPAAjRgXHa21607697 = -845357216;    int ZPAAjRgXHa77382553 = -377664648;    int ZPAAjRgXHa70089205 = -731548758;    int ZPAAjRgXHa64506441 = -390998783;    int ZPAAjRgXHa47221391 = 43650868;    int ZPAAjRgXHa61074952 = 30206593;    int ZPAAjRgXHa71636713 = -460033718;    int ZPAAjRgXHa53643440 = 53443882;    int ZPAAjRgXHa68200826 = -73515085;    int ZPAAjRgXHa83583878 = -442467819;    int ZPAAjRgXHa29770397 = -700516112;    int ZPAAjRgXHa79314506 = -328872801;    int ZPAAjRgXHa97899499 = -899220481;    int ZPAAjRgXHa55501513 = -193116440;    int ZPAAjRgXHa9471146 = -607357052;    int ZPAAjRgXHa70730329 = -728373400;    int ZPAAjRgXHa39297173 = -166411971;    int ZPAAjRgXHa70669332 = -604641307;    int ZPAAjRgXHa31523531 = -61641097;    int ZPAAjRgXHa56992773 = -423079099;    int ZPAAjRgXHa82385648 = -465102620;    int ZPAAjRgXHa64311779 = -491575843;    int ZPAAjRgXHa90132614 = -104267843;    int ZPAAjRgXHa45086295 = -27051892;    int ZPAAjRgXHa97587302 = -821850353;    int ZPAAjRgXHa11342370 = -734626764;    int ZPAAjRgXHa2056998 = -612645752;     ZPAAjRgXHa25151519 = ZPAAjRgXHa7474905;     ZPAAjRgXHa7474905 = ZPAAjRgXHa63091843;     ZPAAjRgXHa63091843 = ZPAAjRgXHa58140006;     ZPAAjRgXHa58140006 = ZPAAjRgXHa69478439;     ZPAAjRgXHa69478439 = ZPAAjRgXHa77993286;     ZPAAjRgXHa77993286 = ZPAAjRgXHa7987218;     ZPAAjRgXHa7987218 = ZPAAjRgXHa28341313;     ZPAAjRgXHa28341313 = ZPAAjRgXHa54604049;     ZPAAjRgXHa54604049 = ZPAAjRgXHa2825343;     ZPAAjRgXHa2825343 = ZPAAjRgXHa12516893;     ZPAAjRgXHa12516893 = ZPAAjRgXHa61159736;     ZPAAjRgXHa61159736 = ZPAAjRgXHa77157543;     ZPAAjRgXHa77157543 = ZPAAjRgXHa76053103;     ZPAAjRgXHa76053103 = ZPAAjRgXHa8317232;     ZPAAjRgXHa8317232 = ZPAAjRgXHa51413169;     ZPAAjRgXHa51413169 = ZPAAjRgXHa26840933;     ZPAAjRgXHa26840933 = ZPAAjRgXHa51485129;     ZPAAjRgXHa51485129 = ZPAAjRgXHa88361973;     ZPAAjRgXHa88361973 = ZPAAjRgXHa14993286;     ZPAAjRgXHa14993286 = ZPAAjRgXHa74191055;     ZPAAjRgXHa74191055 = ZPAAjRgXHa45429464;     ZPAAjRgXHa45429464 = ZPAAjRgXHa67621423;     ZPAAjRgXHa67621423 = ZPAAjRgXHa86365833;     ZPAAjRgXHa86365833 = ZPAAjRgXHa90893942;     ZPAAjRgXHa90893942 = ZPAAjRgXHa52929860;     ZPAAjRgXHa52929860 = ZPAAjRgXHa75550789;     ZPAAjRgXHa75550789 = ZPAAjRgXHa60509100;     ZPAAjRgXHa60509100 = ZPAAjRgXHa69325757;     ZPAAjRgXHa69325757 = ZPAAjRgXHa71268328;     ZPAAjRgXHa71268328 = ZPAAjRgXHa93767680;     ZPAAjRgXHa93767680 = ZPAAjRgXHa89283009;     ZPAAjRgXHa89283009 = ZPAAjRgXHa77116529;     ZPAAjRgXHa77116529 = ZPAAjRgXHa26623734;     ZPAAjRgXHa26623734 = ZPAAjRgXHa19019228;     ZPAAjRgXHa19019228 = ZPAAjRgXHa10522815;     ZPAAjRgXHa10522815 = ZPAAjRgXHa8249540;     ZPAAjRgXHa8249540 = ZPAAjRgXHa24238837;     ZPAAjRgXHa24238837 = ZPAAjRgXHa98934226;     ZPAAjRgXHa98934226 = ZPAAjRgXHa66754276;     ZPAAjRgXHa66754276 = ZPAAjRgXHa37610733;     ZPAAjRgXHa37610733 = ZPAAjRgXHa4101851;     ZPAAjRgXHa4101851 = ZPAAjRgXHa80923023;     ZPAAjRgXHa80923023 = ZPAAjRgXHa20400032;     ZPAAjRgXHa20400032 = ZPAAjRgXHa25290881;     ZPAAjRgXHa25290881 = ZPAAjRgXHa19257230;     ZPAAjRgXHa19257230 = ZPAAjRgXHa99286420;     ZPAAjRgXHa99286420 = ZPAAjRgXHa7349963;     ZPAAjRgXHa7349963 = ZPAAjRgXHa76925221;     ZPAAjRgXHa76925221 = ZPAAjRgXHa39555361;     ZPAAjRgXHa39555361 = ZPAAjRgXHa91953822;     ZPAAjRgXHa91953822 = ZPAAjRgXHa95868180;     ZPAAjRgXHa95868180 = ZPAAjRgXHa33781497;     ZPAAjRgXHa33781497 = ZPAAjRgXHa67645384;     ZPAAjRgXHa67645384 = ZPAAjRgXHa55893405;     ZPAAjRgXHa55893405 = ZPAAjRgXHa79722055;     ZPAAjRgXHa79722055 = ZPAAjRgXHa39853482;     ZPAAjRgXHa39853482 = ZPAAjRgXHa76726009;     ZPAAjRgXHa76726009 = ZPAAjRgXHa67246063;     ZPAAjRgXHa67246063 = ZPAAjRgXHa16548579;     ZPAAjRgXHa16548579 = ZPAAjRgXHa2442498;     ZPAAjRgXHa2442498 = ZPAAjRgXHa47478118;     ZPAAjRgXHa47478118 = ZPAAjRgXHa59015556;     ZPAAjRgXHa59015556 = ZPAAjRgXHa83335720;     ZPAAjRgXHa83335720 = ZPAAjRgXHa9057663;     ZPAAjRgXHa9057663 = ZPAAjRgXHa23233883;     ZPAAjRgXHa23233883 = ZPAAjRgXHa84043206;     ZPAAjRgXHa84043206 = ZPAAjRgXHa50533809;     ZPAAjRgXHa50533809 = ZPAAjRgXHa57033875;     ZPAAjRgXHa57033875 = ZPAAjRgXHa97794417;     ZPAAjRgXHa97794417 = ZPAAjRgXHa43163630;     ZPAAjRgXHa43163630 = ZPAAjRgXHa2602097;     ZPAAjRgXHa2602097 = ZPAAjRgXHa52550902;     ZPAAjRgXHa52550902 = ZPAAjRgXHa21607697;     ZPAAjRgXHa21607697 = ZPAAjRgXHa77382553;     ZPAAjRgXHa77382553 = ZPAAjRgXHa70089205;     ZPAAjRgXHa70089205 = ZPAAjRgXHa64506441;     ZPAAjRgXHa64506441 = ZPAAjRgXHa47221391;     ZPAAjRgXHa47221391 = ZPAAjRgXHa61074952;     ZPAAjRgXHa61074952 = ZPAAjRgXHa71636713;     ZPAAjRgXHa71636713 = ZPAAjRgXHa53643440;     ZPAAjRgXHa53643440 = ZPAAjRgXHa68200826;     ZPAAjRgXHa68200826 = ZPAAjRgXHa83583878;     ZPAAjRgXHa83583878 = ZPAAjRgXHa29770397;     ZPAAjRgXHa29770397 = ZPAAjRgXHa79314506;     ZPAAjRgXHa79314506 = ZPAAjRgXHa97899499;     ZPAAjRgXHa97899499 = ZPAAjRgXHa55501513;     ZPAAjRgXHa55501513 = ZPAAjRgXHa9471146;     ZPAAjRgXHa9471146 = ZPAAjRgXHa70730329;     ZPAAjRgXHa70730329 = ZPAAjRgXHa39297173;     ZPAAjRgXHa39297173 = ZPAAjRgXHa70669332;     ZPAAjRgXHa70669332 = ZPAAjRgXHa31523531;     ZPAAjRgXHa31523531 = ZPAAjRgXHa56992773;     ZPAAjRgXHa56992773 = ZPAAjRgXHa82385648;     ZPAAjRgXHa82385648 = ZPAAjRgXHa64311779;     ZPAAjRgXHa64311779 = ZPAAjRgXHa90132614;     ZPAAjRgXHa90132614 = ZPAAjRgXHa45086295;     ZPAAjRgXHa45086295 = ZPAAjRgXHa97587302;     ZPAAjRgXHa97587302 = ZPAAjRgXHa11342370;     ZPAAjRgXHa11342370 = ZPAAjRgXHa2056998;     ZPAAjRgXHa2056998 = ZPAAjRgXHa25151519;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void gdtuLWZvbz20703546() {     int IiNkMuWFdS83010547 = -968278332;    int IiNkMuWFdS27949851 = -207625694;    int IiNkMuWFdS26621174 = -437586733;    int IiNkMuWFdS75210328 = -316038097;    int IiNkMuWFdS6257758 = -799636165;    int IiNkMuWFdS59488410 = -741221665;    int IiNkMuWFdS73705691 = -781150041;    int IiNkMuWFdS27678632 = -472564344;    int IiNkMuWFdS87804457 = -952344169;    int IiNkMuWFdS16212613 = -189968393;    int IiNkMuWFdS47599276 = -712316228;    int IiNkMuWFdS89047479 = -624956337;    int IiNkMuWFdS18694541 = -855251252;    int IiNkMuWFdS64120278 = -862063212;    int IiNkMuWFdS12902808 = -513520502;    int IiNkMuWFdS30081288 = 24770531;    int IiNkMuWFdS20794702 = -362188596;    int IiNkMuWFdS11220671 = -409330768;    int IiNkMuWFdS89337394 = -171467651;    int IiNkMuWFdS43199262 = -332406831;    int IiNkMuWFdS28822178 = -482773176;    int IiNkMuWFdS56073675 = -851126164;    int IiNkMuWFdS67683332 = -117958805;    int IiNkMuWFdS19358349 = 82127316;    int IiNkMuWFdS77566762 = -593780681;    int IiNkMuWFdS18782873 = -88018082;    int IiNkMuWFdS42617620 = -560568644;    int IiNkMuWFdS36341925 = -68474862;    int IiNkMuWFdS97529639 = 4043792;    int IiNkMuWFdS33779179 = -644190965;    int IiNkMuWFdS89816359 = -584078157;    int IiNkMuWFdS15199504 = 94711277;    int IiNkMuWFdS99468364 = -878381811;    int IiNkMuWFdS15195173 = -986259697;    int IiNkMuWFdS96340970 = -95579224;    int IiNkMuWFdS18120148 = -216630067;    int IiNkMuWFdS27218333 = -201605106;    int IiNkMuWFdS89244981 = -810771884;    int IiNkMuWFdS7005727 = -359394149;    int IiNkMuWFdS7465738 = -393032625;    int IiNkMuWFdS24108376 = -496084350;    int IiNkMuWFdS10580047 = -588264986;    int IiNkMuWFdS55289873 = -211159402;    int IiNkMuWFdS41511693 = -453579753;    int IiNkMuWFdS25254502 = -524238697;    int IiNkMuWFdS92462903 = -738653733;    int IiNkMuWFdS75533396 = -925238160;    int IiNkMuWFdS18907788 = -130321223;    int IiNkMuWFdS56961014 = -171640249;    int IiNkMuWFdS21077850 = 71773893;    int IiNkMuWFdS55075023 = -683620695;    int IiNkMuWFdS24110396 = -705927639;    int IiNkMuWFdS29820352 = -714621805;    int IiNkMuWFdS23423235 = -401323325;    int IiNkMuWFdS91881367 = -495824804;    int IiNkMuWFdS26936873 = -17152169;    int IiNkMuWFdS60266518 = 10333110;    int IiNkMuWFdS7262825 = -419714050;    int IiNkMuWFdS97643566 = -722257417;    int IiNkMuWFdS87474885 = -611618084;    int IiNkMuWFdS16870791 = -80653021;    int IiNkMuWFdS37363766 = -612675180;    int IiNkMuWFdS30148993 = -376608136;    int IiNkMuWFdS54025278 = -208153205;    int IiNkMuWFdS26396253 = -605890236;    int IiNkMuWFdS32399773 = -707027505;    int IiNkMuWFdS89579115 = -746574526;    int IiNkMuWFdS3499369 = -868991555;    int IiNkMuWFdS67779308 = -666483989;    int IiNkMuWFdS94782659 = -196890435;    int IiNkMuWFdS2862956 = -773624363;    int IiNkMuWFdS31549720 = -551416712;    int IiNkMuWFdS4214945 = 50063381;    int IiNkMuWFdS81871656 = -778435027;    int IiNkMuWFdS19090886 = -836322481;    int IiNkMuWFdS18242131 = -894508191;    int IiNkMuWFdS783803 = -539966762;    int IiNkMuWFdS26171640 = -664379052;    int IiNkMuWFdS94103847 = -393633988;    int IiNkMuWFdS85103858 = -855126948;    int IiNkMuWFdS43249476 = -162779923;    int IiNkMuWFdS23709833 = -330247421;    int IiNkMuWFdS79380911 = -896834614;    int IiNkMuWFdS76451789 = 32269899;    int IiNkMuWFdS78704155 = -960570271;    int IiNkMuWFdS65705964 = -878150519;    int IiNkMuWFdS85379152 = -190666918;    int IiNkMuWFdS76045130 = -377058486;    int IiNkMuWFdS23313805 = -390434893;    int IiNkMuWFdS69404098 = 21572945;    int IiNkMuWFdS57853630 = -126963178;    int IiNkMuWFdS19955508 = -781891057;    int IiNkMuWFdS91601415 = 11485533;    int IiNkMuWFdS19530841 = -747776066;    int IiNkMuWFdS90594947 = -212379604;    int IiNkMuWFdS86744610 = -883409171;    int IiNkMuWFdS80431054 = -111656850;    int IiNkMuWFdS1264595 = 96993802;    int IiNkMuWFdS15115441 = -847689517;    int IiNkMuWFdS92854729 = -968278332;     IiNkMuWFdS83010547 = IiNkMuWFdS27949851;     IiNkMuWFdS27949851 = IiNkMuWFdS26621174;     IiNkMuWFdS26621174 = IiNkMuWFdS75210328;     IiNkMuWFdS75210328 = IiNkMuWFdS6257758;     IiNkMuWFdS6257758 = IiNkMuWFdS59488410;     IiNkMuWFdS59488410 = IiNkMuWFdS73705691;     IiNkMuWFdS73705691 = IiNkMuWFdS27678632;     IiNkMuWFdS27678632 = IiNkMuWFdS87804457;     IiNkMuWFdS87804457 = IiNkMuWFdS16212613;     IiNkMuWFdS16212613 = IiNkMuWFdS47599276;     IiNkMuWFdS47599276 = IiNkMuWFdS89047479;     IiNkMuWFdS89047479 = IiNkMuWFdS18694541;     IiNkMuWFdS18694541 = IiNkMuWFdS64120278;     IiNkMuWFdS64120278 = IiNkMuWFdS12902808;     IiNkMuWFdS12902808 = IiNkMuWFdS30081288;     IiNkMuWFdS30081288 = IiNkMuWFdS20794702;     IiNkMuWFdS20794702 = IiNkMuWFdS11220671;     IiNkMuWFdS11220671 = IiNkMuWFdS89337394;     IiNkMuWFdS89337394 = IiNkMuWFdS43199262;     IiNkMuWFdS43199262 = IiNkMuWFdS28822178;     IiNkMuWFdS28822178 = IiNkMuWFdS56073675;     IiNkMuWFdS56073675 = IiNkMuWFdS67683332;     IiNkMuWFdS67683332 = IiNkMuWFdS19358349;     IiNkMuWFdS19358349 = IiNkMuWFdS77566762;     IiNkMuWFdS77566762 = IiNkMuWFdS18782873;     IiNkMuWFdS18782873 = IiNkMuWFdS42617620;     IiNkMuWFdS42617620 = IiNkMuWFdS36341925;     IiNkMuWFdS36341925 = IiNkMuWFdS97529639;     IiNkMuWFdS97529639 = IiNkMuWFdS33779179;     IiNkMuWFdS33779179 = IiNkMuWFdS89816359;     IiNkMuWFdS89816359 = IiNkMuWFdS15199504;     IiNkMuWFdS15199504 = IiNkMuWFdS99468364;     IiNkMuWFdS99468364 = IiNkMuWFdS15195173;     IiNkMuWFdS15195173 = IiNkMuWFdS96340970;     IiNkMuWFdS96340970 = IiNkMuWFdS18120148;     IiNkMuWFdS18120148 = IiNkMuWFdS27218333;     IiNkMuWFdS27218333 = IiNkMuWFdS89244981;     IiNkMuWFdS89244981 = IiNkMuWFdS7005727;     IiNkMuWFdS7005727 = IiNkMuWFdS7465738;     IiNkMuWFdS7465738 = IiNkMuWFdS24108376;     IiNkMuWFdS24108376 = IiNkMuWFdS10580047;     IiNkMuWFdS10580047 = IiNkMuWFdS55289873;     IiNkMuWFdS55289873 = IiNkMuWFdS41511693;     IiNkMuWFdS41511693 = IiNkMuWFdS25254502;     IiNkMuWFdS25254502 = IiNkMuWFdS92462903;     IiNkMuWFdS92462903 = IiNkMuWFdS75533396;     IiNkMuWFdS75533396 = IiNkMuWFdS18907788;     IiNkMuWFdS18907788 = IiNkMuWFdS56961014;     IiNkMuWFdS56961014 = IiNkMuWFdS21077850;     IiNkMuWFdS21077850 = IiNkMuWFdS55075023;     IiNkMuWFdS55075023 = IiNkMuWFdS24110396;     IiNkMuWFdS24110396 = IiNkMuWFdS29820352;     IiNkMuWFdS29820352 = IiNkMuWFdS23423235;     IiNkMuWFdS23423235 = IiNkMuWFdS91881367;     IiNkMuWFdS91881367 = IiNkMuWFdS26936873;     IiNkMuWFdS26936873 = IiNkMuWFdS60266518;     IiNkMuWFdS60266518 = IiNkMuWFdS7262825;     IiNkMuWFdS7262825 = IiNkMuWFdS97643566;     IiNkMuWFdS97643566 = IiNkMuWFdS87474885;     IiNkMuWFdS87474885 = IiNkMuWFdS16870791;     IiNkMuWFdS16870791 = IiNkMuWFdS37363766;     IiNkMuWFdS37363766 = IiNkMuWFdS30148993;     IiNkMuWFdS30148993 = IiNkMuWFdS54025278;     IiNkMuWFdS54025278 = IiNkMuWFdS26396253;     IiNkMuWFdS26396253 = IiNkMuWFdS32399773;     IiNkMuWFdS32399773 = IiNkMuWFdS89579115;     IiNkMuWFdS89579115 = IiNkMuWFdS3499369;     IiNkMuWFdS3499369 = IiNkMuWFdS67779308;     IiNkMuWFdS67779308 = IiNkMuWFdS94782659;     IiNkMuWFdS94782659 = IiNkMuWFdS2862956;     IiNkMuWFdS2862956 = IiNkMuWFdS31549720;     IiNkMuWFdS31549720 = IiNkMuWFdS4214945;     IiNkMuWFdS4214945 = IiNkMuWFdS81871656;     IiNkMuWFdS81871656 = IiNkMuWFdS19090886;     IiNkMuWFdS19090886 = IiNkMuWFdS18242131;     IiNkMuWFdS18242131 = IiNkMuWFdS783803;     IiNkMuWFdS783803 = IiNkMuWFdS26171640;     IiNkMuWFdS26171640 = IiNkMuWFdS94103847;     IiNkMuWFdS94103847 = IiNkMuWFdS85103858;     IiNkMuWFdS85103858 = IiNkMuWFdS43249476;     IiNkMuWFdS43249476 = IiNkMuWFdS23709833;     IiNkMuWFdS23709833 = IiNkMuWFdS79380911;     IiNkMuWFdS79380911 = IiNkMuWFdS76451789;     IiNkMuWFdS76451789 = IiNkMuWFdS78704155;     IiNkMuWFdS78704155 = IiNkMuWFdS65705964;     IiNkMuWFdS65705964 = IiNkMuWFdS85379152;     IiNkMuWFdS85379152 = IiNkMuWFdS76045130;     IiNkMuWFdS76045130 = IiNkMuWFdS23313805;     IiNkMuWFdS23313805 = IiNkMuWFdS69404098;     IiNkMuWFdS69404098 = IiNkMuWFdS57853630;     IiNkMuWFdS57853630 = IiNkMuWFdS19955508;     IiNkMuWFdS19955508 = IiNkMuWFdS91601415;     IiNkMuWFdS91601415 = IiNkMuWFdS19530841;     IiNkMuWFdS19530841 = IiNkMuWFdS90594947;     IiNkMuWFdS90594947 = IiNkMuWFdS86744610;     IiNkMuWFdS86744610 = IiNkMuWFdS80431054;     IiNkMuWFdS80431054 = IiNkMuWFdS1264595;     IiNkMuWFdS1264595 = IiNkMuWFdS15115441;     IiNkMuWFdS15115441 = IiNkMuWFdS92854729;     IiNkMuWFdS92854729 = IiNkMuWFdS83010547;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void hzTgsFUUmk63565161() {     int AFPpClafTJ70211437 = -578285923;    int AFPpClafTJ18323596 = -122481931;    int AFPpClafTJ11494799 = 56515421;    int AFPpClafTJ37429224 = -132917481;    int AFPpClafTJ30426858 = -144530343;    int AFPpClafTJ85789204 = -363163975;    int AFPpClafTJ89592604 = -744550905;    int AFPpClafTJ39387620 = -536400721;    int AFPpClafTJ57879975 = -417878216;    int AFPpClafTJ3272935 = -759494502;    int AFPpClafTJ60753105 = -516574356;    int AFPpClafTJ85680328 = -688580246;    int AFPpClafTJ12257067 = -994785543;    int AFPpClafTJ22184641 = -119954117;    int AFPpClafTJ79940472 = -531960375;    int AFPpClafTJ50785314 = -89929381;    int AFPpClafTJ76748403 = -337643268;    int AFPpClafTJ73663251 = -548263709;    int AFPpClafTJ29033304 = -397962989;    int AFPpClafTJ84692291 = -957211594;    int AFPpClafTJ7986969 = -512389659;    int AFPpClafTJ83618387 = -601251582;    int AFPpClafTJ2679831 = -4870594;    int AFPpClafTJ62151436 = -419368717;    int AFPpClafTJ28557882 = -136726463;    int AFPpClafTJ70612381 = -213998740;    int AFPpClafTJ71645247 = -528461790;    int AFPpClafTJ39796050 = -12610734;    int AFPpClafTJ6269480 = 26120402;    int AFPpClafTJ33207804 = -365167558;    int AFPpClafTJ12205722 = 18152124;    int AFPpClafTJ81328395 = -710212568;    int AFPpClafTJ24327262 = -306415458;    int AFPpClafTJ38684475 = -834868944;    int AFPpClafTJ77996514 = -4175193;    int AFPpClafTJ8552413 = -587598455;    int AFPpClafTJ14337008 = -611002512;    int AFPpClafTJ71376318 = -881677151;    int AFPpClafTJ71857534 = -842906404;    int AFPpClafTJ24946794 = -867479152;    int AFPpClafTJ38388403 = -296498154;    int AFPpClafTJ74272912 = 22462184;    int AFPpClafTJ40973124 = -205986466;    int AFPpClafTJ15346952 = -326336397;    int AFPpClafTJ13282116 = -477987540;    int AFPpClafTJ99690710 = -289424635;    int AFPpClafTJ53346962 = -290284352;    int AFPpClafTJ40234983 = -92491156;    int AFPpClafTJ50213743 = -235565997;    int AFPpClafTJ14218275 = -691239417;    int AFPpClafTJ63266269 = -687846401;    int AFPpClafTJ25028363 = -999033797;    int AFPpClafTJ46231899 = -322726539;    int AFPpClafTJ99224050 = -775711285;    int AFPpClafTJ55578192 = -15178873;    int AFPpClafTJ86593050 = -977034342;    int AFPpClafTJ15643766 = -17611337;    int AFPpClafTJ49343362 = -524115863;    int AFPpClafTJ8871343 = -996191019;    int AFPpClafTJ59814476 = -930531604;    int AFPpClafTJ14143958 = -834702186;    int AFPpClafTJ49796555 = -631940171;    int AFPpClafTJ33118141 = -462521124;    int AFPpClafTJ24672172 = 47289341;    int AFPpClafTJ91067213 = -677646627;    int AFPpClafTJ79424709 = -806361789;    int AFPpClafTJ61353067 = -282164788;    int AFPpClafTJ73572591 = -59916600;    int AFPpClafTJ44188126 = -15778925;    int AFPpClafTJ71388060 = -944361920;    int AFPpClafTJ36448306 = -478926869;    int AFPpClafTJ5372086 = -455966118;    int AFPpClafTJ1805718 = -705357305;    int AFPpClafTJ4086510 = -530483838;    int AFPpClafTJ46303889 = -560713441;    int AFPpClafTJ33714056 = -434851844;    int AFPpClafTJ42645264 = -295265117;    int AFPpClafTJ87332878 = -678534197;    int AFPpClafTJ48869321 = -941381177;    int AFPpClafTJ28867172 = -847301828;    int AFPpClafTJ17265420 = -923714388;    int AFPpClafTJ31410265 = -335970634;    int AFPpClafTJ89582306 = -777044738;    int AFPpClafTJ92051204 = -282640181;    int AFPpClafTJ69941534 = -677321157;    int AFPpClafTJ87177358 = 17185921;    int AFPpClafTJ35096497 = -287486029;    int AFPpClafTJ25103211 = -530704173;    int AFPpClafTJ83106282 = -719690071;    int AFPpClafTJ91403464 = -27140852;    int AFPpClafTJ92908647 = -469987119;    int AFPpClafTJ64993646 = 13113350;    int AFPpClafTJ62504975 = -885486132;    int AFPpClafTJ12043058 = -912374801;    int AFPpClafTJ10802837 = 67223034;    int AFPpClafTJ88591848 = -664557983;    int AFPpClafTJ41154772 = -515016693;    int AFPpClafTJ16300952 = -153275807;    int AFPpClafTJ24279739 = -648689771;    int AFPpClafTJ33857406 = -578285923;     AFPpClafTJ70211437 = AFPpClafTJ18323596;     AFPpClafTJ18323596 = AFPpClafTJ11494799;     AFPpClafTJ11494799 = AFPpClafTJ37429224;     AFPpClafTJ37429224 = AFPpClafTJ30426858;     AFPpClafTJ30426858 = AFPpClafTJ85789204;     AFPpClafTJ85789204 = AFPpClafTJ89592604;     AFPpClafTJ89592604 = AFPpClafTJ39387620;     AFPpClafTJ39387620 = AFPpClafTJ57879975;     AFPpClafTJ57879975 = AFPpClafTJ3272935;     AFPpClafTJ3272935 = AFPpClafTJ60753105;     AFPpClafTJ60753105 = AFPpClafTJ85680328;     AFPpClafTJ85680328 = AFPpClafTJ12257067;     AFPpClafTJ12257067 = AFPpClafTJ22184641;     AFPpClafTJ22184641 = AFPpClafTJ79940472;     AFPpClafTJ79940472 = AFPpClafTJ50785314;     AFPpClafTJ50785314 = AFPpClafTJ76748403;     AFPpClafTJ76748403 = AFPpClafTJ73663251;     AFPpClafTJ73663251 = AFPpClafTJ29033304;     AFPpClafTJ29033304 = AFPpClafTJ84692291;     AFPpClafTJ84692291 = AFPpClafTJ7986969;     AFPpClafTJ7986969 = AFPpClafTJ83618387;     AFPpClafTJ83618387 = AFPpClafTJ2679831;     AFPpClafTJ2679831 = AFPpClafTJ62151436;     AFPpClafTJ62151436 = AFPpClafTJ28557882;     AFPpClafTJ28557882 = AFPpClafTJ70612381;     AFPpClafTJ70612381 = AFPpClafTJ71645247;     AFPpClafTJ71645247 = AFPpClafTJ39796050;     AFPpClafTJ39796050 = AFPpClafTJ6269480;     AFPpClafTJ6269480 = AFPpClafTJ33207804;     AFPpClafTJ33207804 = AFPpClafTJ12205722;     AFPpClafTJ12205722 = AFPpClafTJ81328395;     AFPpClafTJ81328395 = AFPpClafTJ24327262;     AFPpClafTJ24327262 = AFPpClafTJ38684475;     AFPpClafTJ38684475 = AFPpClafTJ77996514;     AFPpClafTJ77996514 = AFPpClafTJ8552413;     AFPpClafTJ8552413 = AFPpClafTJ14337008;     AFPpClafTJ14337008 = AFPpClafTJ71376318;     AFPpClafTJ71376318 = AFPpClafTJ71857534;     AFPpClafTJ71857534 = AFPpClafTJ24946794;     AFPpClafTJ24946794 = AFPpClafTJ38388403;     AFPpClafTJ38388403 = AFPpClafTJ74272912;     AFPpClafTJ74272912 = AFPpClafTJ40973124;     AFPpClafTJ40973124 = AFPpClafTJ15346952;     AFPpClafTJ15346952 = AFPpClafTJ13282116;     AFPpClafTJ13282116 = AFPpClafTJ99690710;     AFPpClafTJ99690710 = AFPpClafTJ53346962;     AFPpClafTJ53346962 = AFPpClafTJ40234983;     AFPpClafTJ40234983 = AFPpClafTJ50213743;     AFPpClafTJ50213743 = AFPpClafTJ14218275;     AFPpClafTJ14218275 = AFPpClafTJ63266269;     AFPpClafTJ63266269 = AFPpClafTJ25028363;     AFPpClafTJ25028363 = AFPpClafTJ46231899;     AFPpClafTJ46231899 = AFPpClafTJ99224050;     AFPpClafTJ99224050 = AFPpClafTJ55578192;     AFPpClafTJ55578192 = AFPpClafTJ86593050;     AFPpClafTJ86593050 = AFPpClafTJ15643766;     AFPpClafTJ15643766 = AFPpClafTJ49343362;     AFPpClafTJ49343362 = AFPpClafTJ8871343;     AFPpClafTJ8871343 = AFPpClafTJ59814476;     AFPpClafTJ59814476 = AFPpClafTJ14143958;     AFPpClafTJ14143958 = AFPpClafTJ49796555;     AFPpClafTJ49796555 = AFPpClafTJ33118141;     AFPpClafTJ33118141 = AFPpClafTJ24672172;     AFPpClafTJ24672172 = AFPpClafTJ91067213;     AFPpClafTJ91067213 = AFPpClafTJ79424709;     AFPpClafTJ79424709 = AFPpClafTJ61353067;     AFPpClafTJ61353067 = AFPpClafTJ73572591;     AFPpClafTJ73572591 = AFPpClafTJ44188126;     AFPpClafTJ44188126 = AFPpClafTJ71388060;     AFPpClafTJ71388060 = AFPpClafTJ36448306;     AFPpClafTJ36448306 = AFPpClafTJ5372086;     AFPpClafTJ5372086 = AFPpClafTJ1805718;     AFPpClafTJ1805718 = AFPpClafTJ4086510;     AFPpClafTJ4086510 = AFPpClafTJ46303889;     AFPpClafTJ46303889 = AFPpClafTJ33714056;     AFPpClafTJ33714056 = AFPpClafTJ42645264;     AFPpClafTJ42645264 = AFPpClafTJ87332878;     AFPpClafTJ87332878 = AFPpClafTJ48869321;     AFPpClafTJ48869321 = AFPpClafTJ28867172;     AFPpClafTJ28867172 = AFPpClafTJ17265420;     AFPpClafTJ17265420 = AFPpClafTJ31410265;     AFPpClafTJ31410265 = AFPpClafTJ89582306;     AFPpClafTJ89582306 = AFPpClafTJ92051204;     AFPpClafTJ92051204 = AFPpClafTJ69941534;     AFPpClafTJ69941534 = AFPpClafTJ87177358;     AFPpClafTJ87177358 = AFPpClafTJ35096497;     AFPpClafTJ35096497 = AFPpClafTJ25103211;     AFPpClafTJ25103211 = AFPpClafTJ83106282;     AFPpClafTJ83106282 = AFPpClafTJ91403464;     AFPpClafTJ91403464 = AFPpClafTJ92908647;     AFPpClafTJ92908647 = AFPpClafTJ64993646;     AFPpClafTJ64993646 = AFPpClafTJ62504975;     AFPpClafTJ62504975 = AFPpClafTJ12043058;     AFPpClafTJ12043058 = AFPpClafTJ10802837;     AFPpClafTJ10802837 = AFPpClafTJ88591848;     AFPpClafTJ88591848 = AFPpClafTJ41154772;     AFPpClafTJ41154772 = AFPpClafTJ16300952;     AFPpClafTJ16300952 = AFPpClafTJ24279739;     AFPpClafTJ24279739 = AFPpClafTJ33857406;     AFPpClafTJ33857406 = AFPpClafTJ70211437;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void TgLkqPcWfY54184246() {     int bTYbLaRtYu28070466 = -933918503;    int bTYbLaRtYu38798541 = -966386342;    int bTYbLaRtYu75024129 = 65451729;    int bTYbLaRtYu54499546 = -319394423;    int bTYbLaRtYu67206177 = 95524453;    int bTYbLaRtYu67284329 = 12745375;    int bTYbLaRtYu55311078 = 97808989;    int bTYbLaRtYu38724939 = -142876872;    int bTYbLaRtYu91080383 = -263113523;    int bTYbLaRtYu16660205 = -893309091;    int bTYbLaRtYu95835488 = -396262342;    int bTYbLaRtYu13568072 = -232489586;    int bTYbLaRtYu53794064 = 77801581;    int bTYbLaRtYu10251816 = 66279820;    int bTYbLaRtYu84526047 = -737437399;    int bTYbLaRtYu29453433 = -541817229;    int bTYbLaRtYu70702172 = -402668501;    int bTYbLaRtYu33398794 = -279291225;    int bTYbLaRtYu30008725 = -713664002;    int bTYbLaRtYu12898268 = -412358211;    int bTYbLaRtYu62618091 = -740218909;    int bTYbLaRtYu94262599 = -172670423;    int bTYbLaRtYu2741740 = -667686;    int bTYbLaRtYu95143952 = -632345133;    int bTYbLaRtYu15230701 = -28883079;    int bTYbLaRtYu36465394 = -530057914;    int bTYbLaRtYu38712079 = -590732997;    int bTYbLaRtYu15628875 = -952903385;    int bTYbLaRtYu34473362 = -700090890;    int bTYbLaRtYu95718653 = -969107847;    int bTYbLaRtYu8254401 = -940958046;    int bTYbLaRtYu7244890 = -351611813;    int bTYbLaRtYu46679097 = -579466499;    int bTYbLaRtYu27255914 = -370326142;    int bTYbLaRtYu55318256 = -600404016;    int bTYbLaRtYu16149747 = -958027645;    int bTYbLaRtYu33305801 = -9339748;    int bTYbLaRtYu36382463 = -741432845;    int bTYbLaRtYu79929033 = -569465880;    int bTYbLaRtYu65658255 = -150102355;    int bTYbLaRtYu24886047 = -392986937;    int bTYbLaRtYu80751108 = -42407634;    int bTYbLaRtYu15339974 = -728437327;    int bTYbLaRtYu36458613 = -714103569;    int bTYbLaRtYu13245737 = -267123376;    int bTYbLaRtYu72896384 = -886488021;    int bTYbLaRtYu29593938 = -390119721;    int bTYbLaRtYu51792807 = -998030028;    int bTYbLaRtYu30249536 = -721491853;    int bTYbLaRtYu95740763 = 49763281;    int bTYbLaRtYu26387470 = -660089221;    int bTYbLaRtYu53270578 = -879213929;    int bTYbLaRtYu42270754 = 33424693;    int bTYbLaRtYu55001901 = -179060891;    int bTYbLaRtYu91566155 = -988574579;    int bTYbLaRtYu33807867 = -661248081;    int bTYbLaRtYu36056802 = -865718656;    int bTYbLaRtYu79880177 = -302203138;    int bTYbLaRtYu39268846 = -190511344;    int bTYbLaRtYu30740783 = -374417634;    int bTYbLaRtYu28572251 = -396521628;    int bTYbLaRtYu39682203 = 50712374;    int bTYbLaRtYu4251578 = -442785982;    int bTYbLaRtYu95361729 = -294005676;    int bTYbLaRtYu8405804 = -952351045;    int bTYbLaRtYu88590598 = 55349471;    int bTYbLaRtYu66888975 = -653023087;    int bTYbLaRtYu26538151 = -551872277;    int bTYbLaRtYu54933559 = -333316164;    int bTYbLaRtYu68376301 = -779409754;    int bTYbLaRtYu96147631 = -432477482;    int bTYbLaRtYu34319709 = -661235656;    int bTYbLaRtYu53469760 = -709825345;    int bTYbLaRtYu64350469 = -463561648;    int bTYbLaRtYu88012221 = 80628725;    int bTYbLaRtYu81866982 = -597811276;    int bTYbLaRtYu78922625 = -444233097;    int bTYbLaRtYu66283127 = -286564117;    int bTYbLaRtYu81898215 = -265221758;    int bTYbLaRtYu42334317 = -142395059;    int bTYbLaRtYu6871456 = -39938193;    int bTYbLaRtYu86919271 = -592702970;    int bTYbLaRtYu85379339 = -131411533;    int bTYbLaRtYu38732598 = -649854171;    int bTYbLaRtYu69331184 = -209018627;    int bTYbLaRtYu54983823 = 38255883;    int bTYbLaRtYu64974136 = -285036507;    int bTYbLaRtYu91677195 = -300405608;    int bTYbLaRtYu35689758 = -381751564;    int bTYbLaRtYu21510390 = -939155936;    int bTYbLaRtYu80092945 = 7691010;    int bTYbLaRtYu53425624 = -707136610;    int bTYbLaRtYu97113617 = -450921501;    int bTYbLaRtYu49188250 = -95048247;    int bTYbLaRtYu37086005 = -753580727;    int bTYbLaRtYu85203843 = -343699312;    int bTYbLaRtYu76499531 = -599621652;    int bTYbLaRtYu19978245 = -334431652;    int bTYbLaRtYu28052809 = -761752524;    int bTYbLaRtYu24655138 = -933918503;     bTYbLaRtYu28070466 = bTYbLaRtYu38798541;     bTYbLaRtYu38798541 = bTYbLaRtYu75024129;     bTYbLaRtYu75024129 = bTYbLaRtYu54499546;     bTYbLaRtYu54499546 = bTYbLaRtYu67206177;     bTYbLaRtYu67206177 = bTYbLaRtYu67284329;     bTYbLaRtYu67284329 = bTYbLaRtYu55311078;     bTYbLaRtYu55311078 = bTYbLaRtYu38724939;     bTYbLaRtYu38724939 = bTYbLaRtYu91080383;     bTYbLaRtYu91080383 = bTYbLaRtYu16660205;     bTYbLaRtYu16660205 = bTYbLaRtYu95835488;     bTYbLaRtYu95835488 = bTYbLaRtYu13568072;     bTYbLaRtYu13568072 = bTYbLaRtYu53794064;     bTYbLaRtYu53794064 = bTYbLaRtYu10251816;     bTYbLaRtYu10251816 = bTYbLaRtYu84526047;     bTYbLaRtYu84526047 = bTYbLaRtYu29453433;     bTYbLaRtYu29453433 = bTYbLaRtYu70702172;     bTYbLaRtYu70702172 = bTYbLaRtYu33398794;     bTYbLaRtYu33398794 = bTYbLaRtYu30008725;     bTYbLaRtYu30008725 = bTYbLaRtYu12898268;     bTYbLaRtYu12898268 = bTYbLaRtYu62618091;     bTYbLaRtYu62618091 = bTYbLaRtYu94262599;     bTYbLaRtYu94262599 = bTYbLaRtYu2741740;     bTYbLaRtYu2741740 = bTYbLaRtYu95143952;     bTYbLaRtYu95143952 = bTYbLaRtYu15230701;     bTYbLaRtYu15230701 = bTYbLaRtYu36465394;     bTYbLaRtYu36465394 = bTYbLaRtYu38712079;     bTYbLaRtYu38712079 = bTYbLaRtYu15628875;     bTYbLaRtYu15628875 = bTYbLaRtYu34473362;     bTYbLaRtYu34473362 = bTYbLaRtYu95718653;     bTYbLaRtYu95718653 = bTYbLaRtYu8254401;     bTYbLaRtYu8254401 = bTYbLaRtYu7244890;     bTYbLaRtYu7244890 = bTYbLaRtYu46679097;     bTYbLaRtYu46679097 = bTYbLaRtYu27255914;     bTYbLaRtYu27255914 = bTYbLaRtYu55318256;     bTYbLaRtYu55318256 = bTYbLaRtYu16149747;     bTYbLaRtYu16149747 = bTYbLaRtYu33305801;     bTYbLaRtYu33305801 = bTYbLaRtYu36382463;     bTYbLaRtYu36382463 = bTYbLaRtYu79929033;     bTYbLaRtYu79929033 = bTYbLaRtYu65658255;     bTYbLaRtYu65658255 = bTYbLaRtYu24886047;     bTYbLaRtYu24886047 = bTYbLaRtYu80751108;     bTYbLaRtYu80751108 = bTYbLaRtYu15339974;     bTYbLaRtYu15339974 = bTYbLaRtYu36458613;     bTYbLaRtYu36458613 = bTYbLaRtYu13245737;     bTYbLaRtYu13245737 = bTYbLaRtYu72896384;     bTYbLaRtYu72896384 = bTYbLaRtYu29593938;     bTYbLaRtYu29593938 = bTYbLaRtYu51792807;     bTYbLaRtYu51792807 = bTYbLaRtYu30249536;     bTYbLaRtYu30249536 = bTYbLaRtYu95740763;     bTYbLaRtYu95740763 = bTYbLaRtYu26387470;     bTYbLaRtYu26387470 = bTYbLaRtYu53270578;     bTYbLaRtYu53270578 = bTYbLaRtYu42270754;     bTYbLaRtYu42270754 = bTYbLaRtYu55001901;     bTYbLaRtYu55001901 = bTYbLaRtYu91566155;     bTYbLaRtYu91566155 = bTYbLaRtYu33807867;     bTYbLaRtYu33807867 = bTYbLaRtYu36056802;     bTYbLaRtYu36056802 = bTYbLaRtYu79880177;     bTYbLaRtYu79880177 = bTYbLaRtYu39268846;     bTYbLaRtYu39268846 = bTYbLaRtYu30740783;     bTYbLaRtYu30740783 = bTYbLaRtYu28572251;     bTYbLaRtYu28572251 = bTYbLaRtYu39682203;     bTYbLaRtYu39682203 = bTYbLaRtYu4251578;     bTYbLaRtYu4251578 = bTYbLaRtYu95361729;     bTYbLaRtYu95361729 = bTYbLaRtYu8405804;     bTYbLaRtYu8405804 = bTYbLaRtYu88590598;     bTYbLaRtYu88590598 = bTYbLaRtYu66888975;     bTYbLaRtYu66888975 = bTYbLaRtYu26538151;     bTYbLaRtYu26538151 = bTYbLaRtYu54933559;     bTYbLaRtYu54933559 = bTYbLaRtYu68376301;     bTYbLaRtYu68376301 = bTYbLaRtYu96147631;     bTYbLaRtYu96147631 = bTYbLaRtYu34319709;     bTYbLaRtYu34319709 = bTYbLaRtYu53469760;     bTYbLaRtYu53469760 = bTYbLaRtYu64350469;     bTYbLaRtYu64350469 = bTYbLaRtYu88012221;     bTYbLaRtYu88012221 = bTYbLaRtYu81866982;     bTYbLaRtYu81866982 = bTYbLaRtYu78922625;     bTYbLaRtYu78922625 = bTYbLaRtYu66283127;     bTYbLaRtYu66283127 = bTYbLaRtYu81898215;     bTYbLaRtYu81898215 = bTYbLaRtYu42334317;     bTYbLaRtYu42334317 = bTYbLaRtYu6871456;     bTYbLaRtYu6871456 = bTYbLaRtYu86919271;     bTYbLaRtYu86919271 = bTYbLaRtYu85379339;     bTYbLaRtYu85379339 = bTYbLaRtYu38732598;     bTYbLaRtYu38732598 = bTYbLaRtYu69331184;     bTYbLaRtYu69331184 = bTYbLaRtYu54983823;     bTYbLaRtYu54983823 = bTYbLaRtYu64974136;     bTYbLaRtYu64974136 = bTYbLaRtYu91677195;     bTYbLaRtYu91677195 = bTYbLaRtYu35689758;     bTYbLaRtYu35689758 = bTYbLaRtYu21510390;     bTYbLaRtYu21510390 = bTYbLaRtYu80092945;     bTYbLaRtYu80092945 = bTYbLaRtYu53425624;     bTYbLaRtYu53425624 = bTYbLaRtYu97113617;     bTYbLaRtYu97113617 = bTYbLaRtYu49188250;     bTYbLaRtYu49188250 = bTYbLaRtYu37086005;     bTYbLaRtYu37086005 = bTYbLaRtYu85203843;     bTYbLaRtYu85203843 = bTYbLaRtYu76499531;     bTYbLaRtYu76499531 = bTYbLaRtYu19978245;     bTYbLaRtYu19978245 = bTYbLaRtYu28052809;     bTYbLaRtYu28052809 = bTYbLaRtYu24655138;     bTYbLaRtYu24655138 = bTYbLaRtYu28070466;}
// Junk Finished

// Junk Code By Troll Face & Thaisen's Gen
void JiLQoPMZkS97045862() {     int YSBOamJusn15271357 = -543926095;    int YSBOamJusn29172286 = -881242578;    int YSBOamJusn59897754 = -540446116;    int YSBOamJusn16718442 = -136273807;    int YSBOamJusn91375276 = -349369725;    int YSBOamJusn93585122 = -709196935;    int YSBOamJusn71197991 = -965591875;    int YSBOamJusn50433927 = -206713249;    int YSBOamJusn61155901 = -828647570;    int YSBOamJusn3720527 = -362835200;    int YSBOamJusn8989317 = -200520471;    int YSBOamJusn10200921 = -296113495;    int YSBOamJusn47356589 = -61732711;    int YSBOamJusn68316177 = -291611085;    int YSBOamJusn51563713 = -755877272;    int YSBOamJusn50157459 = -656517141;    int YSBOamJusn26655873 = -378123174;    int YSBOamJusn95841374 = -418224166;    int YSBOamJusn69704634 = -940159340;    int YSBOamJusn54391297 = 62837025;    int YSBOamJusn41782882 = -769835392;    int YSBOamJusn21807312 = 77204158;    int YSBOamJusn37738238 = -987579476;    int YSBOamJusn37937039 = -33841166;    int YSBOamJusn66221820 = -671828861;    int YSBOamJusn88294902 = -656038571;    int YSBOamJusn67739705 = -558626143;    int YSBOamJusn19083000 = -897039257;    int YSBOamJusn43213202 = -678014279;    int YSBOamJusn95147278 = -690084440;    int YSBOamJusn30643762 = -338727765;    int YSBOamJusn73373781 = -56535658;    int YSBOamJusn71537994 = -7500146;    int YSBOamJusn50745215 = -218935389;    int YSBOamJusn36973800 = -508999985;    int YSBOamJusn6582011 = -228996033;    int YSBOamJusn20424477 = -418737154;    int YSBOamJusn18513800 = -812338112;    int YSBOamJusn44780841 = 47021865;    int YSBOamJusn83139311 = -624548881;    int YSBOamJusn39166074 = -193400741;    int YSBOamJusn44443975 = -531680464;    int YSBOamJusn1023225 = -723264391;    int YSBOamJusn10293872 = -586860213;    int YSBOamJusn1273351 = -220872219;    int YSBOamJusn80124191 = -437258923;    int YSBOamJusn7407504 = -855165913;    int YSBOamJusn73120003 = -960199961;    int YSBOamJusn23502265 = -785417601;    int YSBOamJusn88881189 = -713250029;    int YSBOamJusn34578716 = -664314927;    int YSBOamJusn54188545 = -72320088;    int YSBOamJusn58682300 = -674680041;    int YSBOamJusn30802718 = -553448852;    int YSBOamJusn55262980 = -507928648;    int YSBOamJusn93464044 = -521130253;    int YSBOamJusn91434048 = -893663103;    int YSBOamJusn21960715 = -406604951;    int YSBOamJusn50496621 = -464444946;    int YSBOamJusn3080375 = -693331154;    int YSBOamJusn25845418 = -50570792;    int YSBOamJusn52114992 = 31447382;    int YSBOamJusn7220725 = -528698970;    int YSBOamJusn66008622 = -38563131;    int YSBOamJusn73076764 = 75892564;    int YSBOamJusn35615536 = -43984813;    int YSBOamJusn38662927 = -188613349;    int YSBOamJusn96611373 = -842797322;    int YSBOamJusn31342377 = -782611100;    int YSBOamJusn44981702 = -426881239;    int YSBOamJusn29732983 = -137779988;    int YSBOamJusn8142074 = -565785063;    int YSBOamJusn51060534 = -365246031;    int YSBOamJusn86565323 = -215610460;    int YSBOamJusn15225224 = -743762234;    int YSBOamJusn97338907 = -138154929;    int YSBOamJusn20784088 = -199531452;    int YSBOamJusn27444366 = -300719263;    int YSBOamJusn36663689 = -812968948;    int YSBOamJusn86097629 = -134569939;    int YSBOamJusn80887399 = -800872659;    int YSBOamJusn94619702 = -598426182;    int YSBOamJusn95580734 = -11621657;    int YSBOamJusn54332013 = -964764251;    int YSBOamJusn60568562 = 74230486;    int YSBOamJusn76455217 = -166407678;    int YSBOamJusn14691481 = -381855618;    int YSBOamJusn40735276 = -454051294;    int YSBOamJusn95482235 = -711006741;    int YSBOamJusn43509756 = -987869733;    int YSBOamJusn15147962 = -335332931;    int YSBOamJusn98463761 = 87867797;    int YSBOamJusn68017178 = -247893166;    int YSBOamJusn41700466 = -259646982;    int YSBOamJusn57293894 = -473978089;    int YSBOamJusn87051081 = -124848124;    int YSBOamJusn37223250 = 97018506;    int YSBOamJusn35014602 = -584701260;    int YSBOamJusn37217107 = -562752778;    int YSBOamJusn65657814 = -543926095;     YSBOamJusn15271357 = YSBOamJusn29172286;     YSBOamJusn29172286 = YSBOamJusn59897754;     YSBOamJusn59897754 = YSBOamJusn16718442;     YSBOamJusn16718442 = YSBOamJusn91375276;     YSBOamJusn91375276 = YSBOamJusn93585122;     YSBOamJusn93585122 = YSBOamJusn71197991;     YSBOamJusn71197991 = YSBOamJusn50433927;     YSBOamJusn50433927 = YSBOamJusn61155901;     YSBOamJusn61155901 = YSBOamJusn3720527;     YSBOamJusn3720527 = YSBOamJusn8989317;     YSBOamJusn8989317 = YSBOamJusn10200921;     YSBOamJusn10200921 = YSBOamJusn47356589;     YSBOamJusn47356589 = YSBOamJusn68316177;     YSBOamJusn68316177 = YSBOamJusn51563713;     YSBOamJusn51563713 = YSBOamJusn50157459;     YSBOamJusn50157459 = YSBOamJusn26655873;     YSBOamJusn26655873 = YSBOamJusn95841374;     YSBOamJusn95841374 = YSBOamJusn69704634;     YSBOamJusn69704634 = YSBOamJusn54391297;     YSBOamJusn54391297 = YSBOamJusn41782882;     YSBOamJusn41782882 = YSBOamJusn21807312;     YSBOamJusn21807312 = YSBOamJusn37738238;     YSBOamJusn37738238 = YSBOamJusn37937039;     YSBOamJusn37937039 = YSBOamJusn66221820;     YSBOamJusn66221820 = YSBOamJusn88294902;     YSBOamJusn88294902 = YSBOamJusn67739705;     YSBOamJusn67739705 = YSBOamJusn19083000;     YSBOamJusn19083000 = YSBOamJusn43213202;     YSBOamJusn43213202 = YSBOamJusn95147278;     YSBOamJusn95147278 = YSBOamJusn30643762;     YSBOamJusn30643762 = YSBOamJusn73373781;     YSBOamJusn73373781 = YSBOamJusn71537994;     YSBOamJusn71537994 = YSBOamJusn50745215;     YSBOamJusn50745215 = YSBOamJusn36973800;     YSBOamJusn36973800 = YSBOamJusn6582011;     YSBOamJusn6582011 = YSBOamJusn20424477;     YSBOamJusn20424477 = YSBOamJusn18513800;     YSBOamJusn18513800 = YSBOamJusn44780841;     YSBOamJusn44780841 = YSBOamJusn83139311;     YSBOamJusn83139311 = YSBOamJusn39166074;     YSBOamJusn39166074 = YSBOamJusn44443975;     YSBOamJusn44443975 = YSBOamJusn1023225;     YSBOamJusn1023225 = YSBOamJusn10293872;     YSBOamJusn10293872 = YSBOamJusn1273351;     YSBOamJusn1273351 = YSBOamJusn80124191;     YSBOamJusn80124191 = YSBOamJusn7407504;     YSBOamJusn7407504 = YSBOamJusn73120003;     YSBOamJusn73120003 = YSBOamJusn23502265;     YSBOamJusn23502265 = YSBOamJusn88881189;     YSBOamJusn88881189 = YSBOamJusn34578716;     YSBOamJusn34578716 = YSBOamJusn54188545;     YSBOamJusn54188545 = YSBOamJusn58682300;     YSBOamJusn58682300 = YSBOamJusn30802718;     YSBOamJusn30802718 = YSBOamJusn55262980;     YSBOamJusn55262980 = YSBOamJusn93464044;     YSBOamJusn93464044 = YSBOamJusn91434048;     YSBOamJusn91434048 = YSBOamJusn21960715;     YSBOamJusn21960715 = YSBOamJusn50496621;     YSBOamJusn50496621 = YSBOamJusn3080375;     YSBOamJusn3080375 = YSBOamJusn25845418;     YSBOamJusn25845418 = YSBOamJusn52114992;     YSBOamJusn52114992 = YSBOamJusn7220725;     YSBOamJusn7220725 = YSBOamJusn66008622;     YSBOamJusn66008622 = YSBOamJusn73076764;     YSBOamJusn73076764 = YSBOamJusn35615536;     YSBOamJusn35615536 = YSBOamJusn38662927;     YSBOamJusn38662927 = YSBOamJusn96611373;     YSBOamJusn96611373 = YSBOamJusn31342377;     YSBOamJusn31342377 = YSBOamJusn44981702;     YSBOamJusn44981702 = YSBOamJusn29732983;     YSBOamJusn29732983 = YSBOamJusn8142074;     YSBOamJusn8142074 = YSBOamJusn51060534;     YSBOamJusn51060534 = YSBOamJusn86565323;     YSBOamJusn86565323 = YSBOamJusn15225224;     YSBOamJusn15225224 = YSBOamJusn97338907;     YSBOamJusn97338907 = YSBOamJusn20784088;     YSBOamJusn20784088 = YSBOamJusn27444366;     YSBOamJusn27444366 = YSBOamJusn36663689;     YSBOamJusn36663689 = YSBOamJusn86097629;     YSBOamJusn86097629 = YSBOamJusn80887399;     YSBOamJusn80887399 = YSBOamJusn94619702;     YSBOamJusn94619702 = YSBOamJusn95580734;     YSBOamJusn95580734 = YSBOamJusn54332013;     YSBOamJusn54332013 = YSBOamJusn60568562;     YSBOamJusn60568562 = YSBOamJusn76455217;     YSBOamJusn76455217 = YSBOamJusn14691481;     YSBOamJusn14691481 = YSBOamJusn40735276;     YSBOamJusn40735276 = YSBOamJusn95482235;     YSBOamJusn95482235 = YSBOamJusn43509756;     YSBOamJusn43509756 = YSBOamJusn15147962;     YSBOamJusn15147962 = YSBOamJusn98463761;     YSBOamJusn98463761 = YSBOamJusn68017178;     YSBOamJusn68017178 = YSBOamJusn41700466;     YSBOamJusn41700466 = YSBOamJusn57293894;     YSBOamJusn57293894 = YSBOamJusn87051081;     YSBOamJusn87051081 = YSBOamJusn37223250;     YSBOamJusn37223250 = YSBOamJusn35014602;     YSBOamJusn35014602 = YSBOamJusn37217107;     YSBOamJusn37217107 = YSBOamJusn65657814;     YSBOamJusn65657814 = YSBOamJusn15271357;}
// Junk Finished
