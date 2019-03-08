#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define	I2S_NODE	"/dev/i2c-0"
#define ARRAY_SIZE(a)	(int)(sizeof(a) / sizeof((a)[0]))
#define	MAX_BUFFER_LEN	1024

static int i2c_open(const char *dev_name)
{
	int fd = open(dev_name, O_RDWR, 644);

  	if (fd < 0) {
    		fprintf(stderr, "Failed, open %s\n", dev_name);
    		return -ENODEV;
  	}

	return fd;
}

static int i2c_close(int fd)
{
	close(fd);
}

static int FD_VALID(int fd)
{
	int ret = fcntl(fd, F_GETFL) != -1 || errno != EBADF;

    	if (!ret)
		fprintf(stderr, "Failed, fd.%d !!!\n", fd);

    	return ret;
}

/*
 * I2C WRITE
 * slave /A buf[0] /A .. buf[n] /NA [S]
 */
static int i2c_msg_w(int fd, int slave, unsigned char *buf, int bytes)
{
	struct i2c_rdwr_ioctl_data rdwr;
	struct i2c_msg msgs[1];
	int ret;

	rdwr.msgs = msgs;
 	rdwr.nmsgs = 1;
 	rdwr.msgs[0].addr = (slave>>1);
 	rdwr.msgs[0].flags = !I2C_M_RD;
 	rdwr.msgs[0].len = (bytes);
 	rdwr.msgs[0].buf = (__u8 *)buf;

	ret = ioctl(fd, I2C_RDWR, &rdwr);
	if (ret < 0)
		return ret;

	return 0;
}

static int i2c_write(int fd, int slave, unsigned char *buf, int bytes)
{
	return i2c_msg_w(fd, slave, buf, bytes);
}

/*
 * I2C WRITE
 * slave /A buf[0] /A .. buf[n] /NA [S]
 */
static int i2c_read(int fd, int slave, unsigned char *buf, int bytes)
{
	struct i2c_rdwr_ioctl_data rdwr;
	struct i2c_msg msgs[1];
	int ret;

	rdwr.msgs = msgs;
 	rdwr.nmsgs = 1;
 	rdwr.msgs[0].addr = (slave>>1);
 	rdwr.msgs[0].flags = I2C_M_RD;
 	rdwr.msgs[0].len = bytes;
 	rdwr.msgs[0].buf = (__u8 *)buf;

	ret = ioctl(fd, I2C_RDWR, &rdwr);
	if (ret < 0)
		return ret;

	return 0;
}

static int i2c_read_stop(int fd, int addr,
			 unsigned char *w_buf, int w_len,
			 unsigned char *r_buf, int r_len)
{
	int ret;

	ret = i2c_write(fd, addr, w_buf, w_len);
	if (ret)
		return ret;

	ret = i2c_read(fd, addr, r_buf, r_len);
	if (ret)
		return ret;

	return 0;
}

static int i2c_read_nostop(int fd, int addr,
			   unsigned char *w_buf, int w_len,
			   unsigned char *r_buf, int r_len)
{
	struct i2c_rdwr_ioctl_data rdwr;
	struct i2c_msg msgs[2];
	int ret;

	rdwr.msgs = msgs;
 	rdwr.nmsgs = 2;

 	rdwr.msgs[0].addr = (addr>>1);
 	rdwr.msgs[0].flags = !I2C_M_RD;
 	rdwr.msgs[0].len = w_len;
 	rdwr.msgs[0].buf = (__u8 *)w_buf;

 	rdwr.msgs[1].addr = (addr>>1);
 	rdwr.msgs[1].flags = I2C_M_RD;
 	rdwr.msgs[1].len = r_len;
 	rdwr.msgs[1].buf = (__u8 *)r_buf;

	ret = ioctl(fd, I2C_RDWR, &rdwr);
	if (ret < 0)
		return ret;

	return 0;
}

static int i2c_probe(int fd, int port, unsigned char s, unsigned char e)
{
	int i, found = 0;

	printf("i2c.%d detect [0x%02x~0x%02x]\n", port, s, e);

	for (i = s; i < e; i += 2) {
		if (!i2c_write(fd, i, NULL, 0)) {
			printf("\t0x%02x [0x%02x:7b]\n", i, i>>1);
			found++;
		}
	}

	if (!found)
		printf("Not found\n");

	return found;
}

static void print_usage(void)
{
    	printf( "usage: i2c_test -p <port> -s <slave> -a <addr>,<addr bytes> -w <data>,.,. -r <bytes>\n"
    		"\n"
        	"\t-p port i2c-n, default 'i2c-0'\n"
            	"\t-s slave(8bit hex)\n"
            	"\t-a addr(hex): <addr>,<addr bytes>, addr bytes is 1 or 2\n"
            	"\t-w write(8bit hex) : 0xN,0xN,...\n"
            	"\t-r read bytes\n"
            	"\t-n nostop read\n"
            	"\t-c loop counts\n"
            	"\t-l read bytes length\n"
            	"\t-d detect i2c.n's devices (0x0 ~ 0xff)\n"
            	"\tEX>\n"
            	"\t write: addr 2byte:0xa0b0, data 2byte: 0x12,34\n"
            	"\t #> i2c_test -p 1 -s 10 -a a0b0,2 -w 12,34\n\n"
            	"\t read : addr 2byte:0xa0b0, data 2byte\n"
            	"\t #> i2c_test -p 1 -s 10 -a a0b0,2 -r 2\n"
		);
}

struct option_t {
	int port;
	unsigned int slave, addr;
	unsigned char buffer[MAX_BUFFER_LEN];
	int addr_bytes, read_bytes, write_bytes;
	bool read_mode, write_mode;
	int loops;
	int length;
	bool nostop;
	bool detect;
} OP = {
  	.port = 0,
  	.slave = 0,
  	.addr = 0,
  	.addr_bytes = 1,
  	.read_bytes = 1,
  	.write_bytes = 0,
	.write_mode = false,
	.read_mode = true,
	.loops = 1,
	.nostop = false,
	.detect = false,
};

static int parse_options(int argc, char **argv, struct option_t *op)
{
	int opt;
	char *ptr;
	int i = 0, ret = 0;

	if (argc == 1) {
		print_usage();
		return -EINVAL;
	}

  	op->port = 0, op->slave = 0, op->addr = 0;
	op->write_mode = false, op->read_mode = true;
	op->length = 1;
	op->loops = 1;
	op->nostop = false;
	op->detect = false;

  	memset(op->buffer, 0, sizeof(MAX_BUFFER_LEN));

	while (-1 != (opt = getopt(argc, argv, "hp:a:s:r:w:m:nl:c:d"))) {
		switch(opt) {
        	case 'p':
        		op->port = strtol(optarg, NULL, 10);
        		break;
        	case 's':
        		sscanf(optarg, "%x", &op->slave);
        		break;
        	case 'a':
        		ptr = strtok(optarg, ",");
        		op->addr = strtol(ptr, NULL, 16);

        		ptr = strtok(NULL, ",");
        		if (ptr)
        			op->addr_bytes = strtol(ptr, NULL, 10);
        		break;
        	case 'r':
       			op->read_bytes = strtol(optarg, NULL, 10);
       			op->read_mode = true;

       			if (op->read_bytes > MAX_BUFFER_LEN) {
				fprintf(stderr,
					"Failed, read bytes %d over max %d\n",
					op->read_bytes, MAX_BUFFER_LEN);
				ret = -EINVAL;
			}
			break;
        	case 'w':
			ptr = strtok(optarg, ",");
			op->buffer[0] = strtol(ptr, NULL, 16);
   			i++;

   			while( ptr = strtok(NULL, ","))
   				op->buffer[i++] = strtol(ptr, NULL, 16);

			op->write_bytes = i;
      			op->write_mode = true;

			if (op->write_bytes > MAX_BUFFER_LEN) {
				fprintf(stderr,
					"Failed, write bytes %d over max %d\n",
					op->write_bytes, MAX_BUFFER_LEN);
				ret = -EINVAL;
			}
       			break;
  		case 'n':
			op->nostop = true;
			break;
        	case 'l':
        		op->length = strtol(optarg, NULL, 10);
        		break;
		case 'c':
			op->loops = strtoll(optarg, NULL, 10);
			break;
		case 'd':
			op->detect = true;
			break;
		case 'h':
		default:
			print_usage();
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

int main(int argc, char **argv)
{
	struct option_t *op = &OP;
  	char dev_name[20] = { I2S_NODE };
  	unsigned char *c, *ptr = op->buffer;
  	int fd;
  	int addr, offs, size;
  	int i, n, cnt, ret;

	ret = parse_options(argc, argv, op);
	if (ret)
		return ret;

	sprintf(dev_name, "/dev/i2c-%d", op->port);

  	fd = i2c_open(dev_name);
	if (!FD_VALID(fd))
		return -EINVAL;

	if (op->detect) {
		unsigned char s = 0, e = 255;

		if (optind < argc)
			sscanf(argv[optind++], "%x", (unsigned int *)&s);

		if (optind < argc)
			sscanf(argv[optind++], "%x", (unsigned int *)&e);

		return i2c_probe(fd, op->port, s, e);
	}

	if (op->write_mode) {
		offs = op->addr_bytes;
		size = op->write_bytes;

		memmove(ptr + offs, ptr, size);

		/* set write address */
		for (i = 0; i < offs; i++)
			ptr[i] = (op->addr >> (8 * (offs - (i + 1)))) & 0xff;

		printf("[W] i2c.%d slave:0x%02x, addr:0x%x, bytes %d:%d\n",
			op->port, op->slave, op->addr, offs, size);

		for (cnt = 0; cnt < op->loops; cnt++) {
			ret = i2c_write(fd, op->slave, ptr, size + offs);
			if (ret) {
				fprintf(stderr,
					"Failed, [W] i2c.%d slave:0x%02x, addr:0x%x, bytes %d\n",
					op->port, op->slave, op->addr, size);
				goto exit;
			}

			if (op->loops > 1) {
				printf("\r[count:%d]", cnt);
				fflush(stdout);
			}
		}
	} else {
		offs = op->addr_bytes;
		size = op->read_bytes;

		printf("[R] i2c.%d slave:0x%02x, addr:0x%x, bytes %d:%d, %s, length:%d\n",
			op->port, op->slave, op->addr, offs, size,
			op->nostop ? "no stop" : "stop", op->length);

		for (cnt = 0; cnt < op->loops; cnt++) {
			for (n = 0, addr = op->addr; n < op->length; n++, addr++) {
				/* set write address */
				for (i = 0; offs > i; i++)
					ptr[i] = (addr >> (8 * (offs - (i + 1)))) & 0xff;

				if (op->nostop)
					ret = i2c_read_nostop(fd, op->slave, ptr, offs, ptr, size);
				else
					ret = i2c_read_stop(fd, op->slave, ptr, offs, ptr, size);

				if (ret) {
					fprintf(stderr,
						"Failed, [R] i2c.%d slave:0x%02x, addr:0x%x, bytes %d\n",
						op->port, op->slave, addr, size);
					goto exit;
				}

				if (op->loops > 1) {
					printf("\r[count:%d]", cnt);
					fflush(stdout);
				}

				if (cnt == op->loops - 1) {
					printf("[R] 0x%02x ", addr);
					for (i = 0; op->read_bytes > i; i++) {
						printf("[0x%02x]", ptr[i]);
						if (!((i + 1) % 8))
							printf("\n");
					}
					printf("\n");
				}
			}
		}
 	}

	return 0;
exit:
	i2c_close(fd);

  	return ret;
}
