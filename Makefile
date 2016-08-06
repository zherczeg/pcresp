ifdef CROSS_COMPILER
CC = $(CROSS_COMPILER)
else
ifndef CC
# default compier
CC = gcc
endif
endif

CFLAGS += -Wall

TARGET = pcresp

BINDIR = bin
SRCDIR = src

OBJS = $(addprefix $(BINDIR)/, main.o load.o match.o)

all: $(BINDIR) $(TARGET)

$(BINDIR) :
	mkdir $(BINDIR)

$(BINDIR)/%.o : $(SRCDIR)/%.c $(BINDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BINDIR)/*.o
	rm -f $(BINDIR)/$(TARGET)

pcresp: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(BINDIR)/$@ -lpcre2-8 -lpthread
