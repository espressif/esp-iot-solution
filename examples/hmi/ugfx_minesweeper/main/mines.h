#ifndef _MINES_H_
#define _MINES_H_

#define MINES_CELL_WIDTH 23  // Number of pixels
#define MINES_CELL_HEIGHT 23 // Number of pixels
#define MINES_FIELD_WIDTH 13 // Number of cells
#define MINES_FIELD_HEIGHT 9 // Number of cells
#define MINES_FLAG_DELAY 150 // Number of milliseconds
#define MINES_MINE_COUNT 35  // Around 15%-20% field/mines ratio is nice!

void minesInit(void);
void minesStart(void);

#define MINES_SHOW_SPLASH true

#if MINES_SHOW_SPLASH
void minesShowSplash(void);
#endif

#endif /* _MINES_H_ */
