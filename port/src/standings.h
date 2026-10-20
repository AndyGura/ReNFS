/* The standings shown after a race (NOTES §4.21). */
#ifndef RENFS_STANDINGS_H
#define RENFS_STANDINGS_H

/* fe_standings_segment 0x6a9d0: an open road's segment (stats/standseg.qfs): the
   player's and the rival's place, times and speeds, or a time trial's against the
   records so far; the segment's two records, taken first (records_segment 0x69608)
   when asked, then white, with the commentary. Until Done, Enter, Esc or two minutes */
void standings_segment(int take_records);

/* fe_standings_track 0x6b938: an open road's three segments together (stats/
   standtrk.qfs): the totals, averages and best top speeds, the track's best total and
   top speed; taken first when asked (0x69980, 0x69e00), with the commentary, then the
   ten best (fe_standings_best 0x69f8c, stats/standbst.qfs) */
void standings_track(int take_records);
/* what race_from_menu 0x3770c and race_menu_finish 0x1b6a0 show after a race: finished
   or not, an open road's last segment (or a circuit) or not; by the race type the
   segment's or the track's standings, a race's or tournament's standings so far, a
   tournament's points given */
void standings_after(int finished, int last);

#endif
