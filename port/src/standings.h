/* The standings shown after a race (NOTES §4.21). */
#ifndef RENFS_STANDINGS_H
#define RENFS_STANDINGS_H

/* fe_standings_segment 0x6a9d0: an open road's segment (stats/standseg.qfs): the
   player's and the rival's place, times and speeds, or a time trial's against the
   records so far; the segment's two records, taken first (records_segment 0x69608)
   when asked, then white, with the commentary. Until Done, Enter, Esc or two minutes */
void standings_segment(int take_records);

#endif
