extern int g_connection;
extern struct window_manager g_window_manager;

static void border_update_window_notifications(uint32_t wid)
{
    int window_count = 0;
    uint32_t window_list[1024] = {};

    if (wid) window_list[window_count++] = wid;

    for (int window_index = 0; window_index < g_window_manager.window.capacity; ++window_index) {
        struct bucket *bucket = g_window_manager.window.buckets[window_index];
        while (bucket) {
            if (bucket->value) {
                struct window *window = bucket->value;
                if (window->border.id) {
                    window_list[window_count++] = window->id;
                }
            }

            bucket = bucket->next;
        }
    }

    SLSRequestNotificationsForWindows(g_connection, window_list, window_count);
}

bool border_should_order_in(struct window *window)
{
    return !window->application->is_hidden && !window_check_flag(window, WINDOW_MINIMIZE) && !window_check_flag(window, WINDOW_FULLSCREEN);
}

void border_show_all(void)
{
    for (int window_index = 0; window_index < g_window_manager.window.capacity; ++window_index) {
        struct bucket *bucket = g_window_manager.window.buckets[window_index];
        while (bucket) {
            if (bucket->value) {
                struct window *window = bucket->value;
                if (window->border.id && border_should_order_in(window)) {
                    SLSOrderWindow(g_connection, window->border.id, -1, window->id);
                }
            }

            bucket = bucket->next;
        }
    }
}

void border_hide_all(void)
{
    for (int window_index = 0; window_index < g_window_manager.window.capacity; ++window_index) {
        struct bucket *bucket = g_window_manager.window.buckets[window_index];
        while (bucket) {
            if (bucket->value) {
                struct window *window = bucket->value;
                if (window->border.id) {
                    SLSOrderWindow(g_connection, window->border.id, 0, 0);
                }
            }

            bucket = bucket->next;
        }
    }
}

void border_redraw(struct window *window)
{
    SLSDisableUpdate(g_connection);
    CGContextClearRect(window->border.context, window->border.frame);
    CGContextAddPath(window->border.context, window->border.path);
    CGContextDrawPath(window->border.context, kCGPathFillStroke);
    CGContextFlush(window->border.context);
    SLSReenableUpdate(g_connection);
}

void border_activate(struct window *window)
{
    if (!window->border.id) return;

    CGContextSetRGBStrokeColor(window->border.context,
                               g_window_manager.active_border_color.r,
                               g_window_manager.active_border_color.g,
                               g_window_manager.active_border_color.b,
                               g_window_manager.active_border_color.a);
    border_redraw(window);
}

void border_deactivate(struct window *window)
{
    if (!window->border.id) return;

    CGContextSetRGBStrokeColor(window->border.context,
                               g_window_manager.normal_border_color.r,
                               g_window_manager.normal_border_color.g,
                               g_window_manager.normal_border_color.b,
                               g_window_manager.normal_border_color.a);
    border_redraw(window);
}

void border_ensure_same_space(struct window *window)
{
    int space_count;
    uint64_t *space_list = window_space_list(window, &space_count);
    if (!space_list) return;

    if (space_count > 1) {
        uint64_t tag = 1ULL << 11;
        SLSSetWindowTags(g_connection, window->border.id, &tag, 64);
    } else {
        uint64_t tag = 1ULL << 11;
        SLSClearWindowTags(g_connection, window->border.id, &tag, 64);
        SLSMoveWindowsToManagedSpace(g_connection, window->border.id_ref, space_list[0]);
    }
}

void border_hide(struct window *window)
{
    if (!window->border.id) return;

    SLSOrderWindow(g_connection, window->border.id, 0, 0);
}

void border_show(struct window *window)
{
    if (!window->border.id) return;

    SLSOrderWindow(g_connection, window->border.id, -1, window->id);
}

void border_resize(struct window *window)
{
    struct border *border = &window->border;

    if (border->region) CFRelease(border->region);
    if (border->path)   CGPathRelease(border->path);
    border->region = NULL;
    border->path = NULL;

    CGRect frame = {};
    SLSGetWindowBounds(g_connection, window->id, &frame);
    CGRect window_border = frame;
    window_border.origin = (CGPoint) {BORDER_OFFSET, BORDER_OFFSET};
    frame = CGRectInset(frame, -BORDER_OFFSET, -BORDER_OFFSET);

    CGSNewRegionWithRect(&frame, &border->region);
    border->frame.size = frame.size;

    if (window_border.size.height > 18 && window_border.size.width > 18) {
      border->path = CGPathCreateMutable();
      CGPathAddRoundedRect(border->path, NULL, window_border, 9, 9);

      SLSOrderWindow(g_connection, border->id, 0, 0);
      SLSSetWindowShape(g_connection, border->id, 0.0f, 0.0f, border->region);
      CGContextClearRect(border->context, border->frame);
      CGContextAddPath(border->context, border->path);
      CGContextDrawPath(window->border.context, kCGPathFillStroke);
      CGContextFlush(border->context);
      SLSOrderWindow(g_connection, border->id, -1, window->id);
    }
}

void SLSSetWindowBackgroundBlurRadius(uint32_t cid, uint32_t wid, uint32_t radius);
void border_create(struct window *window)
{
    if (window->border.id) return;

    if ((!window_rule_check_flag(window, WINDOW_RULE_MANAGED)) &&
        (!window_is_standard(window)) &&
        (!window_is_dialog(window))) {
        return;
    }

    CGRect frame = window->frame;
    CGRect window_border = window->frame;
    window_border.origin = (CGPoint) {BORDER_OFFSET, BORDER_OFFSET};
    frame = CGRectInset(frame, -BORDER_OFFSET, -BORDER_OFFSET);

    CGSNewRegionWithRect(&frame, &window->border.region);
    window->border.frame.size = frame.size;

    uint64_t tag = 1ULL << 46;
    SLSNewWindow(g_connection, 2, 0, 0, window->border.region, &window->border.id);
    SLSSetWindowTags(g_connection, window->border.id, &tag, 64);
    sls_window_disable_shadow(window->border.id);
    SLSSetWindowResolution(g_connection, window->border.id, 1.0f);
    SLSSetWindowOpacity(g_connection, window->border.id, 0);
    SLSSetWindowLevel(g_connection, window->border.id, window_level(window));
    SLSSetWindowBackgroundBlurRadius(g_connection, window->border.id, 20);

    window->border.id_ref = cfarray_of_cfnumbers(&window->border.id, sizeof(uint32_t), 1, kCFNumberSInt32Type);
    window->border.context = SLWindowContextCreate(g_connection, window->border.id, 0);
    CGContextSetLineWidth(window->border.context, g_window_manager.border_width);
    CGContextSetRGBStrokeColor(window->border.context,
                               g_window_manager.normal_border_color.r,
                               g_window_manager.normal_border_color.g,
                               g_window_manager.normal_border_color.b,
                               g_window_manager.normal_border_color.a);
    CGContextSetRGBFillColor(window->border.context, 1, 1, 1, 0x15 / 255.);

    SLSDisableUpdate(g_connection);
    border_resize(window);
    SLSReenableUpdate(g_connection);

    if (border_should_order_in(window)) {
        border_ensure_same_space(window);
        SLSOrderWindow(g_connection, window->border.id, -1, window->id);
    }

    border_update_window_notifications(window->id);
}

void border_destroy(struct window *window)
{
    if (!window->border.id) return;

    if (window->border.id_ref) CFRelease(window->border.id_ref);
    if (window->border.region) CFRelease(window->border.region);
    if (window->border.path)   CGPathRelease(window->border.path);

    CGContextRelease(window->border.context);
    SLSReleaseWindow(g_connection, window->border.id);
    memset(&window->border, 0, sizeof(struct border));

    border_update_window_notifications(0);
}
