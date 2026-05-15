struct _DecodeState
{
	int ini;
	int cnt1, cnt2;
	int pos1;
	int bufptr;
	unsigned char *buffer;
	int(*getByte)();
};

void decodeinit(struct _DecodeState *state);
int decodestep(struct _DecodeState *state);
