#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "guiutils.h"
#include "fprompt.h"

#include "images/icon_ok_20x20.xpm"
#include "images/icon_cancel_20x20.xpm"
#include "images/icon_folder_opened_20x20.xpm"


typedef struct _FPromptData		FPromptData;
#define FPROMPT_DATA(p)			((FPromptData *)(p))
#define FPROMPT_KEY			"FPrompt"


/*
 *	Flags:
 */
typedef enum {
	FPROMPT_MAPPED			= (1 << 0),
	FPROMPT_REALIZED		= (1 << 1),
	FPROMPT_BUTTON_PRESSED_OUTSIDE	= (1 << 8)	/* Records that a
							 * "button_press_event"
							 * occured outside of
							 * the FPrompt */
} FPromptFlags;


/* Callbacks */
static void FPromptDestroyCB(gpointer data);
static gint FPromptEventCB(
	GtkWidget *widget, GdkEvent *event, gpointer data
);
static gint FPromptEntryEventCB(
	GtkWidget *widget, GdkEvent *event, gpointer data
);
static void FPromptRealizeCB(GtkWidget *widget, gpointer data);
static void FPromptBrowseCB(GtkWidget *widget, gpointer data);
static void FPromptOKCB(GtkWidget *widget, gpointer data);
static void FPromptCancelCB(GtkWidget *widget, gpointer data);

static gint FPromptShadowExposeEventCB(
	GtkWidget *widget, GdkEventExpose *expose, gpointer data
);
static void FPromptShadowDrawCB(
	GtkWidget *widget, GdkRectangle *rect, gpointer data
);
static void FPromptShadowDraw(FPromptData *p);
static void FPromptShadowConfigure(
	FPromptData *p,
	const gint x, const gint y,
	const gint width, const gint height
);

gint FPromptInit(void);
void FPromptSetTransientFor(GtkWidget *w);
gboolean FPromptIsQuery(void);
void FPromptBreakQuery(void);
void FPromptSetPosition(const gint x, const gint y);
void FPromptSetShadowStyle(const FPromptShadowStyle shadow_style);
gint FPromptMapQuery(
	const gchar *label,
	const gchar *value,
	const gchar *tooltip_message,
	const FPromptMapRelativity map_relativity,
	const gint width, const gint height,
	const FPromptOptions options,
	gpointer data,
	gchar *(*browse_cb)(gpointer, const gchar *),
	void (*apply_cb)(gpointer, const gchar *),
	void (*cancel_cb)(gpointer)
);
void FPromptMap(void);
void FPromptUnmap(void);
void FPromptShutdown(void);


#define FPROMPT_SHADOW_OFFSET_X		5
#define FPROMPT_SHADOW_OFFSET_Y		5


#define ATOI(s)		(((s) != NULL) ? atoi(s) : 0)
#define ATOL(s)		(((s) != NULL) ? atol(s) : 0)
#define ATOF(s)		(((s) != NULL) ? atof(s) : 0.0f)
#define STRDUP(s)	(((s) != NULL) ? g_strdup(s) : NULL)

#define MAX(a,b)	(((a) > (b)) ? (a) : (b))
#define MIN(a,b)	(((a) < (b)) ? (a) : (b))
#define CLIP(a,l,h)	(MIN(MAX((a),(l)),(h)))
#define STRLEN(s)	(((s) != NULL) ? (gint)strlen(s) : 0)
#define STRISEMPTY(s)	(((s) != NULL) ? (*(s) == '\0') : TRUE)


/*
 *	Floating Prompt:
 */
struct _FPromptData {

	GtkWidget	*toplevel;	/* Popup GtkWindow */
	gint		freeze_count;
	FPromptFlags	flags;

	FPromptShadowStyle	shadow_style;

	GtkWidget	*main_hbox,
			*label,
			*entry,
			*browse_btn,
			*ok_btn,
			*cancel_btn,
			*shadow;	/* Shadow popup GtkWindow */

	GdkPixmap	*shadow_pm;

	gint		x, y,		/* Geometry of toplevel */
			width, height;

	gchar	*(*browse_cb)(
		gpointer,		/* Data */
		const gchar *		/* Value */
	);
	gpointer	browse_data;
	void	(*apply_cb)(
		gpointer,		/* Data */
		const gchar *		/* Value */
	);
	gpointer	apply_data;
	void	(*cancel_cb)(
		gpointer		/* Data */
	);
	gpointer	cancel_data;

};

static FPromptData	*fprompt = NULL;



/*
 *	Toplevel GtkWindow "destroy" signal callback.
 */
static void FPromptDestroyCB(gpointer data)
{
	FPromptData *p = FPROMPT_DATA(data);
	if(p == NULL)
	    return;

	if(p == fprompt)
	    fprompt = NULL;

	p->freeze_count++;

/*	p->toplevel = NULL; */
/*	p->shadow = NULL; */
	GDK_PIXMAP_UNREF(p->shadow_pm);

	p->freeze_count--;

	g_free(p);
}

/*
 *	Toplevel GtkWindow event signal callback.
 */
static gint FPromptEventCB(
	GtkWidget *widget, GdkEvent *event, gpointer data
)
{
	gint status = FALSE;
	GdkEventConfigure *configure;
	FPromptData *p = FPROMPT_DATA(data);
	if((widget == NULL) || (event == NULL) || (p == NULL))
	    return(status);

	switch((gint)event->type)
	{
	  case GDK_DELETE:
	    FPromptCancelCB(widget, data);
	    status = TRUE;
	    break;

	  case GDK_CONFIGURE:
	    configure = (GdkEventConfigure *)event;
	    /* Update the toplevel GtkWindow's geometry */
	    p->x = configure->x;
	    p->y = configure->y;
	    p->width = configure->width;
	    p->height = configure->height;
	    /* Move the shadow GtkWindow to be aligned with the
	     * toplevel GtkWindow
	     */
	    FPromptShadowConfigure(
		p,
		configure->x, configure->y,
		configure->width, configure->height
	    );
	    status = TRUE;
	    break;
	}

	return(status);
}

/*
 *	GtkEntry event signal callback.
 */
static gint FPromptEntryEventCB(
	GtkWidget *widget, GdkEvent *event, gpointer data
)
{
	gint status = FALSE;
	gboolean press;
	GtkWidget *event_widget;
	GdkEventKey *key;
	GdkEventButton *button;
	GdkEventCrossing *crossing;
	FPromptData *p = FPROMPT_DATA(data);
	if((widget == NULL) || (event == NULL) || (p == NULL))
	    return(status);

	/* If the Floating Prompt is no longer mapped then ignore
	 * all events to the GtkEntry
	 */
	if(!(p->flags & FPROMPT_MAPPED))
	    return(status);

	/* Since the GtkEntry is grabbing input events and will
	 * receive all events, we need to get the GtkWidget that this
	 * event actually occured on
	 */
	event_widget = gtk_get_event_widget(event);

	switch((gint)event->type)
	{
	  case GDK_KEY_PRESS:
	  case GDK_KEY_RELEASE:
	    if(p->freeze_count > 0)
		return(status);
	    key = (GdkEventKey *)event;
	    press = (key->type == GDK_KEY_PRESS) ? TRUE : FALSE;
	    switch(key->keyval)
	    {
	      case GDK_Escape:
		if(press)
		{
		    /* Cancel */
		    FPromptCancelCB(p->cancel_btn, p);
		}
		status = TRUE;
		break;

	      case GDK_Return:
	      case GDK_KP_Enter:
	      case GDK_ISO_Enter:
	      case GDK_3270_Enter:
		if(press)
		{
		    /* OK */
		    FPromptOKCB(p->ok_btn, p);
		}
		status = TRUE;
		break;
	    }
	    break;

	  case GDK_BUTTON_PRESS:
	    if(p->freeze_count > 0)
		return(status);
	    button = (GdkEventButton *)event;
	    if((event_widget == p->toplevel) ||
	       (event_widget == p->label) ||
	       (event_widget == widget)
	    )
	    {
		p->flags &= ~FPROMPT_BUTTON_PRESSED_OUTSIDE;
		status = TRUE;
	    }
	    else if((event_widget == p->browse_btn) ||
		    (event_widget == p->ok_btn) ||
		    (event_widget == p->cancel_btn)
	    )
	    {
		p->flags &= ~FPROMPT_BUTTON_PRESSED_OUTSIDE;
		gtk_widget_event(event_widget, event);
		status = TRUE;
	    }
	    else
	    {
		/* Mark that the press occured on other widget
		 * outside of the prompt
		 */
		p->flags |= FPROMPT_BUTTON_PRESSED_OUTSIDE;
	    }
	    break;

	  case GDK_BUTTON_RELEASE:
	    if(p->freeze_count > 0)
		return(status);
	    button = (GdkEventButton *)event;
	    if((event_widget == p->toplevel) ||
	       (event_widget == p->label) ||
	       (event_widget == widget)
	    )
	    {
		p->flags &= ~FPROMPT_BUTTON_PRESSED_OUTSIDE;
		status = TRUE;
	    }
	    else if((event_widget == p->browse_btn) ||
		    (event_widget == p->ok_btn) ||
		    (event_widget == p->cancel_btn)
	    )
	    {
		p->flags &= ~FPROMPT_BUTTON_PRESSED_OUTSIDE;
		gtk_widget_event(event_widget, event);
		status = TRUE;
	    }
	    else
	    {
		/* Released outside of the Prompt, check if the press
		 * was also outside
		 */
		if(p->flags & FPROMPT_BUTTON_PRESSED_OUTSIDE)
		{
		    p->flags &= ~FPROMPT_BUTTON_PRESSED_OUTSIDE;
		    switch(button->button)
		    {
		      case GDK_BUTTON1:
		      case GDK_BUTTON2:
		      case GDK_BUTTON3:
			FPromptCancelCB(p->cancel_btn, p);
			status = TRUE;
			break;
		    }
		}
	    }
	    break;

	  case GDK_ENTER_NOTIFY:
	    crossing = (GdkEventCrossing *)event;
	    if((event_widget == p->toplevel) ||
	       (event_widget == p->label) ||
	       (event_widget == widget)
	    )
	    {
		status = TRUE;
	    }
	    else if((event_widget == p->browse_btn) ||
		    (event_widget == p->ok_btn) ||
		    (event_widget == p->cancel_btn)
	    )
	    {
		gtk_widget_event(event_widget, event);
		status = TRUE;
	    }
	    else
	    {
		/* Need to forward the "enter_notify_event" to the
		 * GtkWidget outside of the Prompt or else GtkButtons
		 * outside of the Prompt will malfunction
		 */
		gtk_widget_event(event_widget, event);
	    }
	    break;

	  case GDK_LEAVE_NOTIFY:
	    crossing = (GdkEventCrossing *)event;
	    if((event_widget == p->toplevel) ||
	       (event_widget == p->label) ||
	       (event_widget == widget)
	    )
	    {
		status = TRUE;
	    }
	    else if((event_widget == p->browse_btn) ||
		    (event_widget == p->ok_btn) ||
		    (event_widget == p->cancel_btn)
	    )
	    {
		gtk_widget_event(event_widget, event);
		status = TRUE;
	    }
	    else
	    {
		/* Need to forward the "leave_notify_event" to the
		 * GtkWidget outside of the Prompt or else GtkButtons
		 * outside of the Prompt will malfunction
		 */
		gtk_widget_event(event_widget, event);
	    }
	    break;
	}

	/* If the Floating Prompt is still mapped and our GtkEntry
	 * list the input grab then we need to regrab input so that
	 * the GtkEntry gets the next event
	 */
	if(p->flags & FPROMPT_MAPPED)
	{
	    /* If our GtkEntry lost the input grab then regrab */
	    if(widget != gtk_grab_get_current())
		gtk_grab_add(widget);
	}

	return(status);
}

/*
 *	Toplevel GtkWindow "realize" signal callback.
 */
static void FPromptRealizeCB(GtkWidget *widget, gpointer data)
{
	FPromptData *p = FPROMPT_DATA(data);
	if((widget == NULL) || (p == NULL))
	    return;  

	/* Mark the Floating Prompt as realized */
	p->flags |= FPROMPT_REALIZED;
}

/*
 *	Browse callback.
 */
static void FPromptBrowseCB(GtkWidget *widget, gpointer data)
{
	FPromptData *p = FPROMPT_DATA(data);
	if((widget == NULL) || (p == NULL))
	    return;  

	if(p->freeze_count > 0)
	    return;

	/* Browse callback set? */
	if(p->browse_cb != NULL)
	{
	    GtkWidget *w = p->entry;

	    /* Get the current value */
	    gchar *v = STRDUP(gtk_entry_get_text(GTK_ENTRY(w)));

	    /* Call the browse callback and get a statically allocated
	     * return string
	     */
	    const gchar *rtn_v = p->browse_cb(
		p->browse_data,		/* Data */
		v			/* Value */
	    );

	    g_free(v);

	    /* Got a new value? */
	    if(rtn_v != NULL)
		gtk_entry_set_text(GTK_ENTRY(w), rtn_v);
	}
}

/*
 *	OK callback.
 */
static void FPromptOKCB(GtkWidget *widget, gpointer data)
{
	FPromptData *p = FPROMPT_DATA(data);
	if(p == NULL)
	    return;

	if(p->freeze_count > 0)
	    return;

	/* Apply callback set? */
	if(p->apply_cb != NULL)
	{
	    GtkWidget *w = p->entry;
	    gchar *v = STRDUP((w != NULL) ?
		gtk_entry_get_text(GTK_ENTRY(w)) : NULL
	    );
	    if(v != NULL)
	    {
		/* Call apply callback */
		p->apply_cb(
		    p->apply_data,	/* Data */
		    v			/* Value */
		);
		g_free(v);
	    }
	    p->apply_cb = NULL;
	    p->apply_data = NULL;
	}

	/* Unmap the prompt */
	FPromptUnmap();

	/* Reset the callbacks */
	p->browse_cb = NULL; 
	p->browse_data = NULL;
	p->apply_cb = NULL;
	p->apply_data = NULL;
	p->cancel_cb = NULL;
	p->cancel_data = NULL;
}

/*
 *	Cancel callback.
 */
static void FPromptCancelCB(GtkWidget *widget, gpointer data)
{
	FPromptData *p = FPROMPT_DATA(data);
	if(p == NULL)
	    return;

	if(p->freeze_count > 0)
	    return;

	/* Call the cancel callback */
	if(p->cancel_cb != NULL)
	    p->cancel_cb(
		p->cancel_data			/* Data */
	    );

	/* Unmap the prompt */
	FPromptUnmap();

	/* Reset the callbacks */
	p->browse_cb = NULL; 
	p->browse_data = NULL;
	p->apply_cb = NULL;
	p->apply_data = NULL;
	p->cancel_cb = NULL;
	p->cancel_data = NULL;
}


/*
 *	Shadow GtkWindow "expose_event" signal callback.
 */
static gint FPromptShadowExposeEventCB(
	GtkWidget *widget, GdkEventExpose *expose, gpointer data
)
{
	FPromptData *p = FPROMPT_DATA(data);
	if((widget == NULL) || (p == NULL))
	    return(FALSE);

	FPromptShadowDraw(p);

	return(TRUE);
}

/*
 *	Shadow GtkWindow "draw" signal callback.
 */
static void FPromptShadowDrawCB(
	GtkWidget *widget, GdkRectangle *rect, gpointer data
)
{
	FPromptData *p = FPROMPT_DATA(data);
	if((widget == NULL) || (p == NULL))
	    return;

	FPromptShadowDraw(p);
}

/*
 *	Draws the shadow.
 */
static void FPromptShadowDraw(FPromptData *p)
{
	gint width, height;
	GdkPixmap *pixmap;
	GdkWindow *window;
	GtkStyle *style;
	GtkWidget *w = p->shadow;
	if(w == NULL)
	    return;

	window = w->window;
	pixmap = p->shadow_pm;
	style = gtk_widget_get_style(w);
	if((window == NULL) || (pixmap == NULL) || (style == NULL))
	    return;

	gdk_window_get_size(
	    (GdkWindow *)pixmap,
	    &width, &height
	);
	if((width <= 0) || (height <= 0))
	    return;

	gdk_draw_pixmap(
	    (GdkDrawable *)window,
	    style->white_gc,
	    (GdkDrawable *)pixmap,
	    0, 0,
	    0, 0,
	    width, height
	);
}

/*
 *	Configures the Floating Prompt's Shadow to the new geometry.
 *
 *	The shadow cast offset will be applied to the specified
 *	position.
 */
static void FPromptShadowConfigure(
	FPromptData *p,
	const gint x, const gint y,
	const gint width, const gint height
)
{
	GtkWidget *w = p->shadow;
	if((w == NULL) || (width <= 0) || (height <= 0))
	    return;

	/* Update the shadow GtkWindow's geometry */
	gtk_widget_set_uposition(
	    w,
	    x + FPROMPT_SHADOW_OFFSET_X,
	    y + FPROMPT_SHADOW_OFFSET_Y
	);
	gtk_widget_set_usize(
	    w,
	    width, height
	);
	gtk_widget_queue_resize(w);
}


/*
 *	Initializes the Floating Prompt.
 */
gint FPromptInit(void)
{
	const gint	border_minor = 2;
	GtkWidget	*w,
			*parent,
			*toplevel,
			*shadow;
	FPromptData *p;

	/* Already initialized */
	if(fprompt != NULL)
	    return(0);

	/* Create the new Floating Prompt */
	p = FPROMPT_DATA(g_malloc0(sizeof(FPromptData)));
	if(p == NULL)
	     return(-3);

	p->toplevel = toplevel = gtk_window_new(GTK_WINDOW_POPUP);
/*
	p->freeze_count = 0;
	p->flags = 0;
 */
	p->shadow_style = FPROMPT_SHADOW_DARKENED;
	p->shadow = shadow = gtk_window_new(GTK_WINDOW_POPUP);
/*	p->shadow_pm = NULL; */
	p->x = 0;
	p->y = 0;
	p->width = FPROMPT_DEF_WIDTH;
	p->height = FPROMPT_DEF_HEIGHT;
/*
	p->browse_cb = NULL;
	p->browse_data = NULL;
	p->apply_cb = NULL;
	p->apply_data = NULL;
	p->cancel_cb = NULL;
	p->cancel_data = NULL;
 */

	p->freeze_count++;

	/* Toplevel GtkWindow */
	w = toplevel;
	gtk_object_set_data_full(
	    GTK_OBJECT(w), FPROMPT_KEY,
	    p, FPromptDestroyCB
	);
	gtk_window_set_policy(GTK_WINDOW(w), FALSE, FALSE, FALSE);
	gtk_widget_add_events(
	    w,
	    GDK_STRUCTURE_MASK |
	    GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
	    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	);
	gtk_signal_connect(
	    GTK_OBJECT(w), "realize",
	    GTK_SIGNAL_FUNC(FPromptRealizeCB), p
	);
	gtk_signal_connect(
	    GTK_OBJECT(w), "delete_event",
	    GTK_SIGNAL_FUNC(FPromptEventCB), p
	);
	gtk_signal_connect(
	    GTK_OBJECT(w), "configure_event",
	    GTK_SIGNAL_FUNC(FPromptEventCB), p
	);
	parent = w;

	/* Main outer GtkFrame */
	w = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(w), GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(parent), w);
	gtk_widget_show(w);
	parent = w;

	/* Main GtkHBox */
	p->main_hbox = w = gtk_hbox_new(FALSE, border_minor);
	gtk_container_add(GTK_CONTAINER(parent), w);
	gtk_widget_show(w);
	parent = w;

	/* GtkLabel */
	p->label = w = gtk_label_new("Value:");
	gtk_label_set_justify(GTK_LABEL(w), GTK_JUSTIFY_RIGHT);
	gtk_box_pack_start(GTK_BOX(parent), w, FALSE, FALSE, 0);
	gtk_widget_show(w);

	/* GtkEntry */
	p->entry = w = gtk_entry_new();
	GTK_WIDGET_SET_FLAGS(w, GTK_CAN_DEFAULT | GTK_CAN_FOCUS);
	/* When connecting signals it is important to connect certain
	 * signals after in order for the widget to to be grabbed
	 * again in the event callback function
	 */
	gtk_widget_add_events(
	    w,
	    GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
	    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
	    GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK
	);
	gtk_signal_connect(
	    GTK_OBJECT(w), "key_press_event",
	    GTK_SIGNAL_FUNC(FPromptEntryEventCB), p  
	);
	gtk_signal_connect(
	    GTK_OBJECT(w), "key_release_event",
	    GTK_SIGNAL_FUNC(FPromptEntryEventCB), p
	);
	gtk_signal_connect_after(
	    GTK_OBJECT(w), "button_press_event",
	    GTK_SIGNAL_FUNC(FPromptEntryEventCB), p
	);
	/* The "button_release_event" needs to be connected after so
	 * that we can catch and check if it occures outside of the
	 * prompt
	 */
	gtk_signal_connect_after(
	    GTK_OBJECT(w), "button_release_event",
	    GTK_SIGNAL_FUNC(FPromptEntryEventCB), p
	);
	gtk_signal_connect_after(
	    GTK_OBJECT(w), "motion_notify_event",
	    GTK_SIGNAL_FUNC(FPromptEntryEventCB), p
	);
	gtk_signal_connect_after(
	    GTK_OBJECT(w), "enter_notify_event",
	    GTK_SIGNAL_FUNC(FPromptEntryEventCB), p
	);
	gtk_signal_connect_after(
	    GTK_OBJECT(w), "leave_notify_event",
	    GTK_SIGNAL_FUNC(FPromptEntryEventCB), p
	);
	gtk_box_pack_start(GTK_BOX(parent), w, TRUE, TRUE, 0);
	gtk_widget_show(w);

	/* Browse GtkButton */
	p->browse_btn = w = GUIButtonPixmap(
	    (guint8 **)icon_folder_opened_20x20_xpm
	);
	gtk_box_pack_start(GTK_BOX(parent), w, FALSE, FALSE, 0);
	gtk_signal_connect(
	    GTK_OBJECT(w), "clicked",
	    GTK_SIGNAL_FUNC(FPromptBrowseCB), p
	);

	/* OK GtkButton */
	p->ok_btn = w = (GtkWidget *)GUIButtonPixmap(
	    (guint8 **)icon_ok_20x20_xpm
	);
	gtk_box_pack_start(GTK_BOX(parent), w, FALSE, FALSE, 0);
	gtk_signal_connect(
	    GTK_OBJECT(w), "clicked",
	    GTK_SIGNAL_FUNC(FPromptOKCB), p
	);

	/* Cancel GtkButton */
	p->cancel_btn = w = (GtkWidget *)GUIButtonPixmap(
	    (guint8 **)icon_cancel_20x20_xpm
	);
	gtk_box_pack_start(GTK_BOX(parent), w, FALSE, FALSE, 0);
	gtk_signal_connect(
	    GTK_OBJECT(w), "clicked",
	    GTK_SIGNAL_FUNC(FPromptCancelCB), p
	);


	/* Shadow GtkWindow */
	w = shadow;
	gtk_window_set_policy(GTK_WINDOW(w), FALSE, FALSE, FALSE);
	gtk_widget_set_app_paintable(w, TRUE);
	gtk_widget_add_events(
	    w,
	    GDK_STRUCTURE_MASK | GDK_EXPOSURE_MASK |
	    GDK_BUTTON_PRESS_MASK		/* So that the GtkEntry gets
						 * grabbed "button_press_event"
						 * signals from the shadow too */
	);
	gtk_signal_connect(
	    GTK_OBJECT(w), "draw",
	    GTK_SIGNAL_FUNC(FPromptShadowDrawCB), p
	);
	gtk_signal_connect(
	    GTK_OBJECT(w), "expose_event",
	    GTK_SIGNAL_FUNC(FPromptShadowExposeEventCB), p
	);

	fprompt = p;

	p->freeze_count--;

	return(0);
}

/*
 *	Sets Floating Prompt as a transient for the given window.
 *
 *	This function has no affect since the Floating Prompt's
 *	GtkWindow is a GTK_WINDOW_POPUP and its entry grabs input,
 *	so there is no need to set it modal or transient for.
 */
void FPromptSetTransientFor(GtkWidget *w)
{

}

/*
 *	Returns TRUE if currently blocking for query.
 */
gboolean FPromptIsQuery(void)
{
	FPromptData *p = fprompt;
	if(p == NULL)
	    return(FALSE);

	return(p->flags & FPROMPT_MAPPED);
}

/*
 *	Ends query if any and returns a not available response.
 */
void FPromptBreakQuery(void)
{
	FPromptData *p = fprompt;
	if(p == NULL)
	    return;

	/* If still in query then call the cancel callback to report
	 * cancel, reset values, and unmap the Floating Prompt
	 */
	if(FPromptIsQuery())
	{
	    /* Call the cancel callback */
	    if(p->cancel_cb != NULL)
		p->cancel_cb(
		    p->cancel_data		/* Data */
		);

	    /* Unmap the prompt */
	    FPromptUnmap();
	}

	/* Reset the callbacks */
	p->browse_cb = NULL;
	p->browse_data = NULL;
	p->apply_cb = NULL;
	p->apply_data = NULL;
	p->cancel_cb = NULL;
	p->cancel_data = NULL;
}

/*
 *	Moves the Floating Prompt to the specified coordinates the
 *	next time it is mapped.
 */
void FPromptSetPosition(gint x, gint y)
{
	GtkWidget *toplevel, *shadow;
	FPromptData *p = fprompt;
	if(p == NULL)
	    return;

	toplevel = p->toplevel;
	shadow = p->shadow;

	gtk_widget_set_uposition(toplevel, x, y);
	gtk_widget_set_uposition(shadow, x, y);

	p->x = x;
	p->y = y;
}

/*
 *	Sets the Floating Prompt's shadow style.
 */
void FPromptSetShadowStyle(const FPromptShadowStyle shadow_style)
{
	FPromptData *p = fprompt;
	if(p == NULL)
	    return;

	p->shadow_style = shadow_style;
}


/*
 *	Maps the Floating Prompt and sets it up with the given values.
 *
 *	Does not block.
 *
 *	Returns 0 on success or non-zero on error.
 */
gint FPromptMapQuery(
	const gchar *label,
	const gchar *value,
	const gchar *tooltip_message,
	const FPromptMapRelativity map_relativity,
	const gint width, const gint height,
	const FPromptOptions options,
	gpointer data,
	gchar *(*browse_cb)(gpointer, const gchar *),
	void (*apply_cb)(gpointer, const gchar *),
	void (*cancel_cb)(gpointer)
)
{
	GtkWidget *w, *toplevel, *shadow;
	FPromptData *p = fprompt;
	if(p == NULL)
	    return(-1);

	toplevel = p->toplevel;
	shadow = p->shadow;

	/* Realize the Floating Prompt's toplevel and shadow GtkWindows
	 * so that their GdkWindows can be used
	 */
	if(!GTK_WIDGET_REALIZED(toplevel))
	    gtk_widget_realize(toplevel);
	if(!GTK_WIDGET_REALIZED(shadow))
	    gtk_widget_realize(shadow);

	/* Unable to realize the Floating Prompt? */
	if(!(p->flags & FPROMPT_REALIZED))
	    return(-1);

	/* Set the label */
	w = p->label;
	if(w != NULL)
	{
	    if(label == NULL)
	    {
		gtk_widget_hide(w);
	    }
	    else
	    {
		gtk_label_set_text(GTK_LABEL(w), label);
		gtk_widget_show(w);
	    }
	}

	/* Set the entry value */
	w = p->entry;
	if(w != NULL)
	{
	    if(value != NULL)
	    {
		const gint len = STRLEN(value);
		gtk_entry_set_text(GTK_ENTRY(w), value);
		if(options & FPROMPT_SELECT_VALUE)
		    gtk_entry_select_region(GTK_ENTRY(w), 0, len);
		gtk_entry_set_position(GTK_ENTRY(w), len);
	    }
	    else
	    {
		/* Leave value it was */
	    }

	    /* Set the tooltip message */
	    GUISetWidgetTip(w, tooltip_message);
	}

	/* Show/hide buttons */
	/* Browse */
	w = p->browse_btn;
	if(w != NULL)
	{
	    if(options & FPROMPT_SHOW_BROWSE_BUTTON)
		gtk_widget_show(w);
	    else
		gtk_widget_hide(w);	    
	}
	/* OK */
	w = p->ok_btn;
	if(w != NULL)
	{
	    if(options & FPROMPT_SHOW_OK_BUTTON)
		gtk_widget_show(w);
	    else
		gtk_widget_hide(w);
	}
	/* Cancel */
	w = p->cancel_btn;
	if(w != NULL)
	{
	    if(options & FPROMPT_SHOW_CANCEL_BUTTON)
		gtk_widget_show(w);
	    else
		gtk_widget_hide(w);
	}


	/* Update the toplevel and shadow GtkWindows */
	if((toplevel != NULL) && (shadow != NULL))
	{
	    gint	x = 0,
			y = 0;
	    GdkWindow	*window,
			*root = gdk_window_get_parent(toplevel->window);

	    /* Record the new size */
	    p->width = (width > 0) ? width : FPROMPT_DEF_WIDTH;
	    p->height = (height > 0) ? height : FPROMPT_DEF_HEIGHT;

	    /* Resize the toplevel and shadow GtkWindows */
	    gtk_widget_set_usize(toplevel, width, height);
	    gtk_widget_set_usize(shadow, width, height);

	    /* Move the toplevel's GdkWindow, calculate x and y
	     * position relative to parent (root) GdkWindow by the
	     * specified map code
	     */
	    switch(map_relativity)
	    {
	      case FPROMPT_MAP_TO_POINTER_WINDOW:
		window = gdk_window_at_pointer(&x, &y);
		if(window != NULL)
		{
		    gint wwidth, wheight;
		    gdk_window_get_root_position(window, &x, &y);
		    gdk_window_get_size(window, &wwidth, &wheight);
		    x += (wwidth / 2) - (p->width / 2);
		    y += (wheight / 2) - (p->height / 2);
		    gtk_widget_set_uposition(toplevel, x, y);
		    gtk_widget_set_uposition(shadow, x, y);
		}
		break;

	      case FPROMPT_MAP_TO_POINTER:
		if(root != NULL)
		{
		    gdk_window_get_pointer(root, &x, &y, 0);
		    x -= (p->width / 2);
		    y -= (p->height / 2);
		}
		gtk_widget_set_uposition(toplevel, x, y);   
		gtk_widget_set_uposition(shadow, x, y);   
		break;

	      case FPROMPT_MAP_TO_POSITION:
		break;
	    }

	    /* Recreate the shadow GdkPixmap */
	    if(p->shadow_style != FPROMPT_SHADOW_NONE)
	    {
		GtkWidget *w = shadow;
		GdkWindow *window = w->window;
		GdkPixmap *pixmap = gdk_pixmap_new(
		    window,
		    p->width, p->height,
		    -1
		);
		GtkStyle *style = gtk_widget_get_style(w);

		GDK_PIXMAP_UNREF(p->shadow_pm);
		p->shadow_pm = pixmap;

		if(pixmap != NULL)
		{
		    gint mask_width = 8, mask_height = 2;
		    guint8 bm_data_8x2[] = { 0x55, 0xaa };
		    GdkBitmap *mask;
		    GdkColor *c, shadow_color;
		    GdkColormap *colormap = gdk_window_get_colormap(window);
		    GdkGC *gc;

		    switch(p->shadow_style)
		    {
		      case FPROMPT_SHADOW_NONE:
			break;

		      case FPROMPT_SHADOW_DITHERED:
			if(root != NULL)
			{
			    gc = gdk_gc_new(window);
			    gdk_gc_set_subwindow(gc, GDK_INCLUDE_INFERIORS);
			    gdk_window_copy_area(
				(GdkWindow *)pixmap,
				gc,
				0, 0,
				root,
				p->x + FPROMPT_SHADOW_OFFSET_X,
				p->y + FPROMPT_SHADOW_OFFSET_Y,
				p->width, p->height
			    );
			    gdk_gc_set_subwindow(gc, GDK_CLIP_BY_CHILDREN);

			    mask = gdk_bitmap_create_from_data(
				window,
				(const gchar *)bm_data_8x2,
				mask_width, mask_height
			    );
			    if(mask != NULL)
			    {
				gint x, y;
				gdk_gc_set_clip_mask(gc, mask);
				gdk_gc_set_foreground(gc, &style->black);
				for(y = 0; y < p->height; y += mask_height)
				{
				    for(x = 0; x < p->width; x += mask_width)
				    {
					gdk_gc_set_clip_origin(gc, x, y);
					gdk_draw_rectangle(
					    (GdkDrawable *)pixmap,
					    gc,
					    TRUE,
					    x, y,
					    mask_width, mask_height
					);
				    }
			        }
				gdk_gc_set_clip_mask(gc, NULL);
			    }
			    GDK_GC_UNREF(gc);
			    GDK_BITMAP_UNREF(mask);
			}
			break;

		      case FPROMPT_SHADOW_BLACK:
			gdk_draw_rectangle(
			    (GdkDrawable *)pixmap,
			    style->black_gc,
			    TRUE,		/* Fill */
			    0, 0,
			    p->width, p->height
			);
			break;

		      case FPROMPT_SHADOW_DARKENED:
			gc = gdk_gc_new(window);
			c = &shadow_color;
			c->red = 0x5fff;
			c->green = 0x5fff;
			c->blue = 0x5fff;
			GDK_COLORMAP_ALLOC_COLOR(colormap, c);

			gdk_gc_set_subwindow(gc, GDK_INCLUDE_INFERIORS);
			gdk_window_copy_area(
			    (GdkWindow *)pixmap,
			    gc,
			    0, 0,
			    root,
			    p->x + FPROMPT_SHADOW_OFFSET_X,
			    p->y + FPROMPT_SHADOW_OFFSET_Y,
			    p->width, p->height
			);
			gdk_gc_set_subwindow(gc, GDK_CLIP_BY_CHILDREN);

			gdk_gc_set_function(gc, GDK_AND);
			gdk_gc_set_foreground(gc, c);
			gdk_draw_rectangle(
			    (GdkDrawable *)pixmap,
			    gc,
			    TRUE,		/* Fill */
			    0, 0,
			    p->width, p->height
			);
			gdk_gc_set_function(gc, GDK_COPY);

			GDK_GC_UNREF(gc);
			GDK_COLORMAP_FREE_COLOR(colormap, c);
			break;
		    }
		}
	    }
	    else
	    {
		GDK_PIXMAP_UNREF(p->shadow_pm);
		p->shadow_pm = NULL;
	    }
	}

	/* Set the callbacks */
	p->browse_cb = browse_cb; 
	p->browse_data = data;
	p->apply_cb = apply_cb;
	p->apply_data = data;
	p->cancel_cb = cancel_cb;
	p->cancel_data = data;

	/* Map the prompt */
	FPromptMap();

	return(0);
}

/*
 *	Maps the Floating Prompt.
 */
void FPromptMap(void)
{
	GtkWidget *w;
	FPromptData *p = fprompt;
	if(p == NULL)
	    return;

	if(p->shadow_style != FPROMPT_SHADOW_NONE)
	    gtk_widget_show_raise(p->shadow);
	gtk_widget_show_raise(p->toplevel);
	p->flags |= FPROMPT_MAPPED;

	/* Set the GtkEntry into focus and have it grab input,
	 * subsequent regrabs for input are needed after each button
	 * or key event, they are taken care of in
	 * FPromptEntryEventCB()
	 */
	w = p->entry;
	gtk_widget_grab_focus(w);
	gtk_widget_grab_default(w);
	if(w != gtk_grab_get_current())
	    gtk_grab_add(w);

	/* Need to manually set the GtkEntry as having focus since
	 * focus events are not sent because its toplevel is a
	 * GTK_WINDOW_POPUP
	 */
	GTK_WIDGET_SET_FLAGS(w, GTK_HAS_FOCUS);
}

/*
 *	Unmaps the Floating Prompt.
 */
void FPromptUnmap(void)
{
	GtkWidget *w;
	FPromptData *p = fprompt;
	if(p == NULL)
	    return;

	/* Remove the grab input from the GtkEntry */
	w = p->entry;
	if(w == gtk_grab_get_current())
	    gtk_grab_remove(w);

	p->x = 0;
	p->y = 0;
	p->width = 0;
	p->height = 0;

	gtk_widget_hide(p->toplevel);
	gtk_widget_hide(p->shadow);
	p->flags &= ~FPROMPT_MAPPED;

	/* Clear the "button_press_event" was outside flag */
	p->flags &= ~FPROMPT_BUTTON_PRESSED_OUTSIDE;

	GDK_PIXMAP_UNREF(p->shadow_pm);
	p->shadow_pm = NULL;
}

/*
 *	Shuts down the Floating Prompt.
 */
void FPromptShutdown(void)
{
	FPromptData *p = fprompt;
	if(p == NULL)
	    return;

	gtk_widget_destroy(p->shadow);

	/* Destroy the toplevel GtkWidget which will call
	 * FPromptDestroyCB() to delete the rest of the FPrompt
	 */
	gtk_widget_destroy(p->toplevel);
}
