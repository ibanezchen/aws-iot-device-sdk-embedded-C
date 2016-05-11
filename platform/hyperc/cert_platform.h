#ifndef CERT_PLATFORM_H_H
#define CERT_PLATFORM_H_H

enum {
	AWS_CERT_ROOT = 0,
	AWS_CERT_CLIENT = 1,
	AWS_PK = 2,
};

typedef struct {
	unsigned short type, sz;
	unsigned char* raw;
}aws_key_t;

aws_key_t* aws_key_init(aws_key_t* o, int type);

void aws_key_dest(aws_key_t* o);

#endif
