#define fmprstr charstr
#define iconsize(x,y) (0)
#define longimagedata(x) (NULL)
#define sizeofimage(x,y,z) (0)
#define setcursor(x,y,z) (0)
#define defcursor(x,y) my_defcursor(x, y)
#define popviewport() my_popviewport()
#define pushviewport() my_pushviewport()
#define getdev(x,y,z) my_getdev(x,y,z)
#define NORMALDRAW 0
#define OVERDRAW 1
#define drawmode(x) frontbuffer(x)
#define noise(x,y) (0)
#define curstype(x) (0)

#define ftrunc floorf

static Screencoord _left, _right, _bottom, _top, _pushed = 0;

static void my_pushviewport(void)
{
    if (_pushed) {
        printf("ERROR: already pushed\n");
    }
    getviewport(&_left, &_right, &_bottom, &_top);
    _pushed = 1;
}

static void my_popviewport(void)
{
    if (!_pushed) {
        printf("ERROR: viewport stack is empty\n");
    }
    viewport(_left, _right, _bottom, _top);
    _pushed = 0;
}

static void my_getdev(Int32 n, Device d[], short v[])
{
    Int32 i;

    for (i = 0; i < n; i++) {
        v[i] = getvaluator(d[i]);
    }
}

static void my_defcursor(Int32 index, Uint16 *cursor)
{
}
