/*
			      File Browser
 */

#ifndef FB_H
#define FB_H

#include <gtk/gtk.h>


typedef struct _fb_type_struct		fb_type_struct;
#define FB_TYPE(p)			((fb_type_struct *)(p))


#define FILE_BROWSER_TOPLEVEL_WIDGET_NAME	\
                                        "file-selection"
#define FILE_BROWSER_LIST_HEADINGS_WIDGET_NAME	\
                                        "list-headings"
#define FILE_BROWSER_CONTENTS_LIST_WIDGET_NAME	\
                                        "contents-list"


/*
 *	File Type:
 */
struct _fb_type_struct {

	gchar		*extensions,		/* Space-separated list of
						 * extensions, example:
						 * ".txt .doc .nfo" (default
						 * extension is first) */
			*name;			/* Verbose name/description */
};


extern gint FileBrowserInit(void);
extern void FileBrowserSetStyle(GtkRcStyle *rc_style);
extern void FileBrowserSetTransientFor(GtkWidget *w);
extern void FileBrowserSetObjectCreatedCB(
	void (*cb)(
		const gchar *path,
		gpointer data
	),
	gpointer data
);
extern void FileBrowserSetObjectModifiedCB(
	void (*cb)(
		const gchar *old_path,
		const gchar *new_path,
		gpointer data
	),
	gpointer data
);
extern void FileBrowserSetObjectDeletedCB(
	void (*cb)(
		const gchar *path,
		gpointer data
	),
	gpointer data
);
extern gboolean FileBrowserIsQuery(void);
extern void FileBrowserBreakQuery(void);
extern GtkWidget *FileBrowserGetToplevel(void);
extern void FileBrowserReset(void);
extern gboolean FileBrowserGetResponse(
	const gchar *title,
	const gchar *ok_label,
	const gchar *cancel_label,
	const gchar *path,
	fb_type_struct **file_types_list, const gint nfile_types,
	gchar ***paths_list_rtn, gint *npaths_rtn,
	fb_type_struct **file_type_rtn
);
extern void FileBrowserMap(void);
extern void FileBrowserUnmap(void);
extern void FileBrowserShutdown(void);

extern const gchar *FileBrowserGetLastLocation(void);
extern void FileBrowserShowHiddenObjects(const gboolean show);
extern void FileBrowserListStandard(void);
extern void FileBrowserListDetailed(void);


/* File Types */
extern fb_type_struct *FileBrowserTypeNew(void);
extern void FileBrowserTypeDelete(fb_type_struct *t);

extern gint FileBrowserTypeListNew(
	fb_type_struct ***list, gint *total,
	const gchar *extensions,		/* Space-separated list of
                                                 * extensions, example:
                                                 * ".txt .doc .nfo" */
	const gchar *name			/* Verbose name/description */
);
extern void FileBrowserDeleteTypeList(
	fb_type_struct **list, const gint total
);

extern gchar *FileBrowserTypeCompleteExtension(
        const gchar *path,
        fb_type_struct *t,
        const gboolean always_replace
);


#endif	/* FB_H */
