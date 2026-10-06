#ifndef RECRAFT_MODAL_INPUT_H
#define RECRAFT_MODAL_INPUT_H
/* A GUI press belongs to the GUI until BOTH buttons are released. */
static int gameplay_input_ready(int *release,int left,int right)
{
    if(!*release) return 1;
    if(left || right) return 0;
    *release=0; return 1;
}
/* An entity strike owns the held press even after that entity is removed. */
static int gameplay_attack_blocks(int *entity_press,int left,int entity_target)
{
    if(!left)*entity_press=0;
    else if(entity_target)*entity_press=1;
    return left && !*entity_press;
}
#endif
