#ifndef RECRAFT_MODAL_INPUT_H
#define RECRAFT_MODAL_INPUT_H
/* A GUI press belongs to the GUI until BOTH buttons are released. */
static int gameplay_input_ready(int *release,int left,int right)
{
    if(!*release) return 1;
    if(left || right) return 0;
    *release=0; return 1;
}
#endif
