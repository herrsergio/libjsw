/*
			      Floating Prompt
 */

#ifndef FPROMPT_H
#define FPROMPT_H

#include <gtk/gtk.h>


/*
 *	Options:
 */
typedef enum {
	FPROMPT_SHOW_LABEL		= (1 << 0),
	FPROMPT_SELECT_VALUE		= (1 << 2),
	FPROMPT_SHOW_BROWSE_BUTTON	= (1 << 8),
	FPROMPT_SHOW_OK_BUTTON		= (1 << 9),
	FPROMPT_SHOW_CANCEL_BUTTON	= (1 << 10)
} FPromptOptions;


/*
 *      Map Relativity:
 */
typedef enum {
	FPROMPT_MAP_TO_POSITION,		/* To the last position set by
						 * FPromptSetPosition() */
	FPROMPT_MAP_TO_POINTER,			/* Centered to pointer */
	FPROMPT_MAP_TO_POINTER_WINDOW		/* Window under the pointer */
} FPromptMapRelativity;


/*
 *	Shadow Styles:
 */
typedef enum {
	FPROMPT_SHADOW_NONE,
	FPROMPT_SHADOW_DITHERED,
	FPROMPT_SHADOW_BLACK,
	FPROMPT_SHADOW_DARKENED
} FPromptShadowStyle;


#define FPROMPT_DEF_WIDTH		250
#define FPROMPT_DEF_HEIGHT		30

#define FPROMPT_BTN_WIDTH		(20 + (2 * 2))
#define FPROMPT_BTN_HEIGHT		(20 + (2 * 2))


extern gint FPromptInit(void);
extern void FPromptSetTransientFor(GtkWidget *w);
extern gboolean FPromptIsQuery(void);
extern void FPromptBreakQuery(void);
extern void FPromptSetPosition(const gint x, const gint y);
extern void FPromptSetShadowStyle(const FPromptShadowStyle shadow_style);
extern gint FPromptMapQuery(
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
extern void FPromptMap(void);
extern void FPromptUnmap(void);
extern void FPromptShutdown(void);


#endif	/* FPROMPT_H */
