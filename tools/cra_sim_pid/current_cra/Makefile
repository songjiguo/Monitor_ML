OUTPUT = proxy
SIM_CLIENT = fake_sim_client
SIM_SERVER = fake_sim_server

all: ab sim

ab: ab_proxy.c ap_release.h
	gcc ab_proxy.c ap_release.h -o $(OUTPUT) -I/usr/local/apr/include/ -L/usr/local/apr/lib -lapr-1 `pkg-config apr-1 --cflags` -laprutil-1 -lm -O3

sim: fake_sim_client.c fake_sim_server.c
	gcc fake_sim_client.c -o $(SIM_CLIENT) -O3
	gcc fake_sim_server.c -o $(SIM_SERVER) -O3

clean:
	rm $(OUTPUT)
	rm $(SIM_CLIENT)
	rm $(SIM_SERVER)
