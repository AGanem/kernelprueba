#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>


#define PORT 8003
t_log* loggercito;
uint32_t pid = 15;
void iniciar_servidor_kernel_prueba();
void* handle_client(void* socket_desc);
uint32_t deserializar_respuesta_resize(t_list*  lista_paquete );
void atender_memoria ( int socket);
int main(int argc, char const *argv[]) {
    loggercito =log_create("kernelprueba.log", "kernelprueba", true, LOG_LEVEL_INFO);
    int fd_memoria = crear_conexion(loggercito,"memoria","127.0.0.1","8002");

     
    pthread_t hilo1, hilo2;

    pthread_create(&hilo1, NULL, atender_memoria, fd_memoria);
    pthread_create(&hilo2, NULL, iniciar_servidor_kernel_prueba, NULL);

    // Esperar a que los hilos terminen (opcional, en caso de que quieras esperar)
    pthread_join(hilo1, NULL);
    pthread_join(hilo2, NULL);

    return 0;
    iniciar_servidor_kernel_prueba();

}    
    void iniciar_servidor_kernel_prueba(){  
        int server_fd, new_socket;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);

        // Creating socket file descriptor
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        // Forcefully attaching socket to the port 8003
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR , &opt, sizeof(opt))) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);

        // Binding the socket to the port 8003
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }
        if (listen(server_fd, 3) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }
        printf("Kernel server is listening on %s:%d...\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        

        while (1) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            printf("Kernel server received connection from client at %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            pthread_t client_thread;
            int* new_sock = malloc(sizeof(int));
            if (new_sock == NULL) {
                perror("malloc failed");
                close(new_socket);
                continue;
            }

            *new_sock = new_socket;
            if (pthread_create(&client_thread, NULL, handle_client, (void*)new_sock) != 0) {
                perror("pthread_create");
                free(new_sock);
                close(new_socket);
                continue;
            }
            pthread_detach(client_thread);  // Detach the thread so that it cleans up after itself
        }

        close(server_fd);
        return 0;
    };

    void* handle_client(void* socket_desc) {
        int new_socket = *(int*)socket_desc;
        free(socket_desc);
        printf("Creating list\n");
        t_list *valores = list_create();
        t_interfaz* interfaz_recibida = malloc(sizeof(t_interfaz));
        printf("Lista creada\n");
        while (1) {
            uint32_t request_type;
            printf("dentro del while \n");
            if (recv(new_socket, &request_type, sizeof(int32_t), MSG_WAITALL) != sizeof(int32_t)) {
                    printf("DISCONNECT!");

                    break;
                }

            printf("Received request type: %u\n", request_type);

            switch (request_type) {

                case HANDSHAKE:
                    printf("Received HANDSHAKE request\n");
                    // Send handshake response
                    uint32_t response_handshake = HANDSHAKE_OK;
                    if (send(new_socket, &response_handshake, sizeof(uint32_t), MSG_WAITALL) != sizeof(uint32_t)) {
                        perror("send handshake response");
                        break;
                    }
                    printf("Kernel server sent handshake response to client\n");
                    break;

                case INTERFAZ_ENVIAR:

                    printf("Received INTERFAZ_ENVIAR request\n");

                    // Assuming recibir_paquete fills the list with the received data
                    valores = recibir_paquete(new_socket);

            
                    interfaz_recibida = deserializar_interfaz(valores);
            
                    if (valores == NULL || list_size(valores) == 0) {
                        printf("Failed to receive data or empty list\n");
                        break;
                    }
                    printf("despues de recbir paquete\n");
                    request_type = (uint32_t)(uintptr_t)list_get(valores, 1);

                    uint32_t response_interfaz = INTERFAZ_RECIBIDA;
                    if (send(new_socket, &response_interfaz, sizeof(uint32_t), 0) != sizeof(uint32_t)) {
                        perror("send INTERFAZ_RECIBIDA response");
                        break;
                    }

                    response_interfaz = HANDSHAKE;
                    if (send(new_socket, &response_interfaz, sizeof(uint32_t), 0) != sizeof(uint32_t)) {
                        perror("send HANDSHAKE response");
                        break;
                    }
                    printf("Kernel server sent HANDSHAKE to client\n");

                    
                    uint32_t tiempo_espera = 100;
                    uint32_t direccion_fisica = 0;
                    uint32_t tamanio_gestion = 3;
                    t_io_espera* io_espera = malloc(sizeof(t_io_espera));
                    t_io_direcciones_fisicas* io_df = malloc(sizeof(t_io_direcciones_fisicas));
                    t_io_gestion_archivo* archivo_nuevo = malloc(sizeof(t_io_espera));
                    t_io_gestion_archivo* archivo_nuevo2 = malloc(sizeof(t_io_espera));
                    t_io_gestion_archivo* archivo_nuevo3 = malloc(sizeof(t_io_espera));
                    io_espera->pid = pid;
                    io_espera->tiempo_espera = tiempo_espera;
                    io_df->direcciones_fisicas= list_create();
                    io_df->pid = pid;
                    list_add(io_df->direcciones_fisicas, direccion_fisica);
                    list_add(io_df->direcciones_fisicas, 0);
                    printf("Kernel agrega direccion fisica a lista\n");
                    io_df->tamanio_operacion = 6;
                    char* nombre_archivo ="notas.txt";
                    uint32_t tamanio_nombre_archivo = string_length(nombre_archivo)+1;
                    archivo_nuevo->pid = pid;
                    archivo_nuevo->nombre_archivo_length = tamanio_nombre_archivo;
                    archivo_nuevo->nombre_archivo = nombre_archivo;
                    archivo_nuevo->tamanio_archivo = 6;
                    printf("Longitud del nombre del archivo %d\n",archivo_nuevo->nombre_archivo_length);

                    char* nombre_archivo2 ="alumnos.txt";
                    uint32_t tamanio_nombre_archivo2 = string_length(nombre_archivo2)+1;
                    archivo_nuevo2->pid = pid;
                    archivo_nuevo2->nombre_archivo_length = tamanio_nombre_archivo2;
                    archivo_nuevo2->nombre_archivo = nombre_archivo2;
                    archivo_nuevo2->tamanio_archivo = 6;


                    char* nombre_archivo3 ="materias.txt";
                    uint32_t tamanio_nombre_archivo3 = string_length(nombre_archivo2)+1;
                    archivo_nuevo3->pid = pid;
                    archivo_nuevo3->nombre_archivo_length = tamanio_nombre_archivo2;
                    archivo_nuevo3->nombre_archivo = nombre_archivo3;
                    archivo_nuevo3->tamanio_archivo = tamanio_gestion;

                    // Armo para operaciones de lecto/escritura hold the door
                    t_io_readwrite_archivo *rw_archivo = malloc(sizeof(t_io_readwrite_archivo));
                    rw_archivo->pid = pid;
                    rw_archivo->nombre_archivo_length = string_length(nombre_archivo2)+1;
                    rw_archivo->nombre_archivo = nombre_archivo;
                    rw_archivo->direcciones_fisicas = list_create();
                    list_add(rw_archivo->direcciones_fisicas, direccion_fisica);
                    list_add(rw_archivo->direcciones_fisicas, 33);
                    rw_archivo->tamanio_operacion = 5;
                    rw_archivo->puntero_archivo = 0;
                    switch (interfaz_recibida->tipo)
                    {
                    case 0:
                        enviar_espera(io_espera, new_socket);
                        break;
                    case 1:
                        enviar_io_df(io_df, new_socket,IO_K_STDIN);
                        printf("Kernel server sent IO_K_STDIN to client\n");
                        break;    
                    case 2:
                        enviar_io_df(io_df, new_socket,IO_K_STDOUT);
                        printf("Kernel server sent IO_K_STDOUT to client\n");
                        break;   
                    case 3:
                        enviar_gestionar_archivo(archivo_nuevo,new_socket,IO_FS_CREATE);
                        printf("Kernel server sent IO_FS_CREATE to client\n");
                        enviar_gestionar_archivo(archivo_nuevo2,new_socket,IO_FS_CREATE);
                        printf("Kernel server sent IO_FS_CREATE to client\n");  
                        enviar_gestionar_archivo(archivo_nuevo3,new_socket,IO_FS_CREATE);
                        printf("Kernel server sent IO_FS_CREATE to client\n");    
                    // enviar_gestionar_archivo(archivo_nuevo,new_socket,IO_FS_DELETE);
                    //   printf("Kernel server sent IO_FS_DELETE to client\n");     // PRUEBA DE BORRADO OK
                        enviar_gestionar_archivo(archivo_nuevo,new_socket,IO_FS_TRUNCATE);
                        printf("Kernel server sent IO_FS_TRUNCATE to client\n");   
                        enviar_io_readwrite(rw_archivo,new_socket,IO_FS_WRITE);
                        printf("Kernel server sent IO_FS_WRITE to client\n");  
                        enviar_gestionar_archivo(archivo_nuevo2,new_socket,IO_FS_TRUNCATE);
                        printf("Kernel server sent IO_FS_TRUNCATE to client\n"); 
                        rw_archivo->tamanio_operacion = 5;  
                        enviar_io_readwrite(rw_archivo,new_socket,IO_FS_READ);
                        printf("Kernel server sent IO_FS_WRITE to client\n");          
                    /* enviar_io_readwrite(rw_archivo,new_socket,IO_FS_WRITE);
                        printf("Kernel server sent IO_FS_WRITE to client\n");
                        enviar_io_readwrite(rw_archivo,new_socket,IO_FS_READ);
                        printf("Kernel server sent IO_FS_WRITE to client\n");   */ // PRUEBAS DE LECTURA Y ESCRITURA OK    
                        break;                                        
                    }
                break;     
                case IO_K_GEN_SLEEP_FIN:
                    printf("Received IO_K_GEN_SLEEP_FIN request\n");
                    
                    break;
                case IO_K_STDIN_FIN:
                    printf("Received IO_K_STDIN_FIN request\n");
                    
                    list_clean(valores);
                    break;   
                case IO_K_STDOUT_FIN:
                    printf("Received IO_K_STDOUT_FIN request\n");
                    
                    list_clean(valores);
                break;                   

                default:
                    fprintf(stderr, "Unknown request type: %u\n", request_type);
                    break;
            }
        }
        printf("SALIO DEL CASE\n");
        list_destroy(valores);
        close(new_socket);
        return NULL;
    }
     

void enviar_crear_proceso_memoria(t_m_crear_proceso* proceso_nuevo, int socket){
 t_paquete* paquete_crear_proceso;
 
    paquete_crear_proceso = crear_paquete(CREAR_PROCESO_KERNEL);
 
    agregar_a_paquete(paquete_crear_proceso, &proceso_nuevo->pid,  sizeof(uint32_t));
    agregar_a_paquete(paquete_crear_proceso, proceso_nuevo->archivo_pseudocodigo, strlen(proceso_nuevo->archivo_pseudocodigo) + 1);  
    
    enviar_paquete(paquete_crear_proceso, socket);   
    printf("Proceso enviado: %s\n", proceso_nuevo->archivo_pseudocodigo); 
    free(paquete_crear_proceso);

}

void enviar_resize_memoria(t_resize* proceso_resize, int socket){
 t_paquete* paquete_resize;
 
    paquete_resize = crear_paquete(SOLICITUD_RESIZE);
 
    agregar_a_paquete(paquete_resize, &proceso_resize->pid,  sizeof(uint32_t));
    agregar_a_paquete(paquete_resize, &proceso_resize->tamanio,  sizeof(uint32_t));
    agregar_a_paquete(paquete_resize, proceso_resize->valor, strlen(proceso_resize->valor) + 1);  
    
    enviar_paquete(paquete_resize, socket);   
    printf("Tamaño resize solicitado: %d\n", proceso_resize->tamanio); 
    free(paquete_resize);

}

uint32_t deserializar_respuesta_resize(t_list*  lista_paquete ){

    //Creamos una variable de tipo struct que ira guardando todo del paquete y le asignamos tamaño
    uint32_t* proceso_a_finalizar = malloc(sizeof(uint32_t));
    
    proceso_a_finalizar = *(uint32_t*)list_get(lista_paquete, 0);
    printf("Respuesta recibida: %d \n", proceso_a_finalizar);

    return proceso_a_finalizar;
}

void atender_memoria ( int socket){
    t_m_crear_proceso* proceso_nuevo= malloc(sizeof(t_m_crear_proceso));
    proceso_nuevo->pid = pid;
    proceso_nuevo->archivo_pseudocodigo = "rutaloca.txt";
    enviar_crear_proceso_memoria( proceso_nuevo, socket);
    uint32_t operacion_memo;
    t_list* paquetes_memo = list_create();
    uint32_t tamanio_resize = 7;    
    int32_t cop;
    while (socket != -1) {

        cop = recibir_operacion(socket); 
   

    switch (cop) {

        case CREAR_PROCESO_KERNEL_FIN: {
            paquetes_memo = recibir_paquete(socket);
            t_m_crear_proceso* proceso_inutil= malloc(sizeof(t_m_crear_proceso));
            proceso_inutil = deserializar_crear_proceso(paquetes_memo);
            list_clean(paquetes_memo);
            printf("MEORIA CREO OK EL PROCESO\n");
            t_resize* proceso_resize = malloc(sizeof(t_resize));
            proceso_resize->pid = pid;
            proceso_resize->tamanio = tamanio_resize;
            proceso_resize->valor = "un valoro";
            enviar_resize_memoria(proceso_resize, socket);
            break;
             }
   case SOLICITUD_RESIZE_RTA:{
        
        paquetes_memo = recibir_paquete(socket);
        uint32_t resp= deserializar_respuesta_resize(paquetes_memo);
        list_clean(paquetes_memo);
        printf("SE REALIZO RESIZE OK\n");
        break;
         } 
     } 
    }   
}             