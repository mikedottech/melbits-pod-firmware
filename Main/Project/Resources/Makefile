OUT_DIR = AudioConverted/
SRC := $(wildcard Audio/*.mp3 Audio/*.wav)
OBJ := $(SRC:.mp3=.ppa)
OBJ2 := $(OBJ:.wav=.ppa)
OBJ3 := $(patsubst Audio/%.ppa,${OUT_DIR}%.ppa,$(OBJ2))
WAVOBJS := $(patsubst Audio/%.ppa,${OUT_DIR}%.wav,$(OBJ2))

.PHONY: all

all: $(OBJ3)
wavs: $(WAVOBJS)

AudioConverted/%.ppa: Audio/%.wav 
	py -3 ../BuildScripts/audiocv.py $< $@
AudioConverted/%.wav: Audio/%.wav
	py -3 ../BuildScripts/audiocv.py -w $< $@

AudioConverted/%.ppa: Audio/%.mp3
	py -3 ../BuildScripts/audiocv.py $< $@
AudioConverted/%.wav: Audio/%.mp3
	py -3 ../BuildScripts/audiocv.py -w $< $@

ifdef ComSpec
    RM=del /F /Q
	OBJ3 := $(subst /,\,$(OBJ3))
	WAVOBJS := $(subst /,\,$(WAVOBJS))
else
    RM=rm -f	
endif

clean:	
	$(RM) $(OBJ3) $(WAVOBJS)
	