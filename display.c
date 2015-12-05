/*
 * display.c
 *
 * AttoEmacs, Hugh Barney, November 2015
 * Derived from: Anthony's Editor January 93, (Public Domain 1991, 1993 by Anthony Howe)
 *
 */

#include "header.h"

/* Reverse scan for start of logical line containing offset */
point_t lnstart(buffer_t *bp, register point_t off)
{
	register char_t *p;
	do
		p = ptr(bp, --off);
	while (bp->b_buf < p && *p != '\n');
	return (bp->b_buf < p ? ++off : 0);
}

/*
 * Forward scan for start of logical line segment containing 'finish'.
 * A segment of a logical line corresponds to a physical screen line.
 */
point_t segstart(buffer_t *bp, point_t start, point_t finish)
{
	char_t *p;
	int c = 0;
	point_t scan = start;

	debug("segstart() p=%d\n", bp->b_point);

	while (scan < finish) {
		p = ptr(bp, scan);
		if (*p == '\n') {
			c = 0;
			start = scan+1;
		} else if (COLS <= c) {
			c = 0;
			start = scan;
		}
		++scan;
		c += *p == '\t' ? 8 - (c & 7) : 1;
	}
	return (c < COLS ? start : finish);
}

/* Forward scan for start of logical line segment following 'finish' */
point_t segnext(buffer_t *bp, point_t start, point_t finish)
{
	char_t *p;
	int c = 0;

	debug("segnext() p=%d\n", bp->b_point);

	point_t scan = segstart(bp, start, finish);
	for (;;) {
		p = ptr(bp, scan);
		if (bp->b_ebuf <= p || COLS <= c)
			break;
		++scan;
		if (*p == '\n')
			break;
		c += *p == '\t' ? 8 - (c & 7) : 1;
	}
	return (p < bp->b_ebuf ? scan : pos(bp, bp->b_ebuf));
}

/* Move up one screen line */
point_t upup(buffer_t *bp, point_t off)
{
	point_t curr = lnstart(bp, off);
	point_t seg = segstart(bp, curr, off);
	if (curr < seg)
		off = segstart(bp, curr, seg-1);
	else
		off = segstart(bp, lnstart(bp,curr-1), curr-1);
	return (off);
}

/* Move down one screen line */
point_t dndn(buffer_t *bp, point_t off)
{
	debug("dndn() p=%d\n", bp->b_point);
	return (segnext(bp, lnstart(bp,off), off));
}

/* Return the offset of a column on the specified line */
point_t lncolumn(buffer_t *bp, point_t offset, int column)
{
	debug("lncolumn() p=%d\n", bp->b_point);

	int c = 0;
	char_t *p;
	while ((p = ptr(bp, offset)) < bp->b_ebuf && *p != '\n' && c < column) {
		c += *p == '\t' ? 8 - (c & 7) : 1;
		++offset;
	}
	return (offset);
}

void display(window_t *wp)
{
	char_t *p;
	int i, j,z;

	buffer_t *bp = wp->w_bufp;

    w2b(wp);  /* update the buffer structure */
	
	/* Re-frame the screen with the screen line containing the point
	 * as the first line, when point < page.  Handles the cases of a
	 * backward scroll or moving to the top of file.  pgup() will
	 * move page relative to point so that page <= point < epage.
	 */
	if (bp->b_point < bp->b_page)
		bp->b_page = segstart(bp, lnstart(bp,bp->b_point), bp->b_point);
	/* Re-frame the whole screen when epage <= point.  Handles the
	 * cases of a forward scroll or redraw.
	 */
	if (bp->b_epage <= bp->b_point) {
		/* Find end of screen plus one. */
		bp->b_page = dndn(bp, bp->b_point);
		/* Number of lines on the screen depends if we are at the
		 * EOF and how many lines are used for help and status.
		 */
		if (pos(bp, bp->b_ebuf) <= bp->b_page) {
			bp->b_page = pos(bp, bp->b_ebuf);
			i = wp->w_rows - 1;
		} else {
			i = wp->w_rows - 0;
		}
		i -= wp->w_top;
		/* Scan backwards the required number of lines. */
		while (0 < i--)
			bp->b_page = upup(bp, bp->b_page);
	}

	move(wp->w_top, 0);
	i = wp->w_top;
	j = 0;
	bp->b_epage = bp->b_page;
	while (1) {
		if (bp->b_point == bp->b_epage) {
			bp->b_row = i;
			bp->b_col = j;
		}
		p = ptr(bp, bp->b_epage);
		if (wp->w_top + wp->w_rows <= i || bp->b_ebuf <= p) /* maxline */
			break;
		if (*p != '\r') {
			if (isprint(*p) || *p == '\t' || *p == '\n') {
				j += *p == '\t' ? 8-(j&7) : 1;
				addch(*p);
			} else {
				const char *ctrl = unctrl(*p);
				j += (int) strlen(ctrl);
				addstr(ctrl);
			}
		}
		if (*p == '\n' || COLS <= j) {
			j -= COLS;
			if (j < 0)
				j = 0;
			++i;
		}
		++bp->b_epage;
	}

	//clrtobot(); // XXX needs replacement

	debug("Display() %si=%d row=%d\n", wp->w_name, i, bp->b_row);
	for (z=i; z < wp->w_top + wp->w_rows; z++) {
		move(z, 0);
		clrtoeol();
		debug("*");
	}
	debug("Display() %si=%d row=%d\n", wp->w_name, i, bp->b_row);

	b2w(wp); /* save buffer stuff on window */
	modeline(wp);
//	dispmsg();
	//move(row, col); /* set cursor */
	//refresh();
}

void modeline(window_t *wp)
{
	int i;
    char lch;
	
    standout();
	move(wp->w_top + wp->w_rows, 0);
    
    lch = (wp == curwp ? '=' : '-');
    addch(lch);
	addch(wp->w_bufp->b_modified ? '*' : lch);
	addstr(" Atto: ");
    addch(lch);
    addch(lch);
    addch(' ');
	addstr(wp->w_name);
    addch(' ');
	addstr(get_buffer_name(wp->w_bufp));
	addch(' ');

	sprintf(temp, "T%dR%d Pt%d Pg%d Pe%d r%dc%d    ", wp->w_top, wp->w_rows, wp->w_point, wp->w_page, wp->w_epage, wp->w_bufp->b_row, wp->w_bufp->b_col);
	addstr(temp);
	
    //i = 14 + strlen(get_buffer_name(wp->w_bufp));
	//	i = 40 + strlen(get_buffer_name(wp->w_bufp));
	/*	
	for (; i<=COLS; i++)
		addch(lch);
	*/	
	standend();
}

void dispmsg()
{
	move(MSGLINE, 0);
	if (msgflag) {
		addstr(msgline);
		msgflag = FALSE;
	}
	clrtoeol();
}


void update_display()
{   
    //window_t *wp;

	/*
    for (wp=wheadp; wp != NULL; wp = wp->w_next)
    {
        if (wp->w_displayed)
            display(wp);
    }
	*/

	if (winp1->w_displayed) {
		debug("draw 1\n");
		display(winp1);
	}
	
	if (winp2->w_displayed) {
		debug("draw 2\n");
		display(winp2);
	}
	
    dispmsg();	
	//move(curwp->w_bufp->b_row, curwp->w_bufp->b_col); /* set cursor off the buffer */
	move(curwp->w_row, curwp->w_col); /* set cursor off the window, saved from redraw */
	refresh();
}

void w2b(window_t *w)
{
	w->w_bufp->b_point = w->w_point;
	//w->w_bufp->b_page = w->w_page;
	//w->w_bufp->b_epage = w->w_epage;
	w->w_bufp->b_row = w->w_row;
	w->w_bufp->b_col = w->w_col;
}

void b2w(window_t *w)
{
	w->w_point = w->w_bufp->b_point;
	//	w->w_page = w->w_bufp->b_page;
	//w->w_epage = w->w_bufp->b_epage;
	w->w_row = w->w_bufp->b_row;
	w->w_col = w->w_bufp->b_col;
}
