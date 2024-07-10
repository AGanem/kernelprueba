#include "utils.h"

t_log* logger;



/*
typedef struct {
    t_log *log;
    int fd;
    char *server_name;
} t_procesar_conexion_args;*/

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)


int iniciar_servidor(t_log* logger, const char* name, char* ip, char* puerto) {
    //printf("ip: %s, puerto: %s", ip, puerto);
    log_info(logger, "comienza iniciar_servidor");
    
    int socket_servidor;
    struct addrinfo hints, *servinfo;

    // Inicializando hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    log_info(logger, "hizo memset");
    //log_info(logger, "ip:%s, puerto:%s", ip,puerto);
    // Recibe los addrinfo
    getaddrinfo(ip, puerto, &hints, &servinfo);
    log_info(logger, "hizo getaddrinfo");

    bool conecto = false;
    log_info(logger, "antes del for");
    // Itera por cada addrinfo devuelto
    for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
        socket_servidor = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_servidor == -1) // fallo de crear socket
            continue;
        int yes=1;
        setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
        if (bind(socket_servidor, p->ai_addr, p->ai_addrlen) == -1) {
            // Si entra aca fallo el bind
            handle_error("bind");
            close(socket_servidor);
            continue;
        }
        // Ni bien conecta uno nos vamos del for
        conecto = true;
        break;
    }
     log_info(logger, "despues del for");

    if(!conecto) {
        free(servinfo);
        return 0;
    }

    listen(socket_servidor, SOMAXCONN); // Escuchando (hasta SOMAXCONN conexiones simultaneas)

    // Aviso al logger
    log_info(logger, "Escuchando en IP %s, puerto: %s (%s)\n", ip, puerto, name);

    freeaddrinfo(servinfo);

    return socket_servidor;
}

int esperar_cliente(t_log* logger, const char* name, int socket_servidor) {
    struct sockaddr_in dir_cliente;
    socklen_t tam_direccion = sizeof(struct sockaddr_in);

    int socket_cliente = accept(socket_servidor, (void*) &dir_cliente, &tam_direccion);

    log_info(logger, "Cliente conectado (a %s)\n", name);

    return socket_cliente;
}

int recibir_operacion(int socket_cliente) // modificar 
{
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void* recibir_buffer(int* size, int socket_cliente)  
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

t_list* recibir_paquete(int socket_cliente) 
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
        
		list_add(valores, valor);        
	}
	free(buffer);

	return valores;    
}
//

void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

int crear_conexion(t_log* logger, const char* server_name, char* ip, char* puerto) {
    struct addrinfo hints, *servinfo;

    // Init de hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Recibe addrinfo
    getaddrinfo(ip, puerto, &hints, &servinfo);

    errno = 0;
    // Crea un socket con la informacion recibida (del primero, suficiente)
    int socket_cliente = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    int yes=1;
    setsockopt(socket_cliente, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    // Fallo en crear el socket
    if(socket_cliente == -1) {
        printf("socket() devolvio: %s \n",strerror(errno));
        log_error(logger, "Error creando el socket para IP %s", ip);
        log_error(logger, "y puerto %s", puerto);
        //freeaddrinfo(servinfo);
        return 0;
    }

    // Error conectando
    if(connect(socket_cliente, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        log_error(logger, "Error al conectar (a %s)\n", server_name);
        freeaddrinfo(servinfo);
        return 0;
    } else
        log_info(logger, "Cliente conectado en %s:%s (a %s)\n", ip, puerto, server_name);

    freeaddrinfo(servinfo);

    return socket_cliente;
}


void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(op_code codigo_operacion)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = codigo_operacion;
	crear_buffer(paquete);
	return paquete;
}
void agregar_a_buffer(t_buffer* un_buffer, void* valor, int tamanio)
{
	//si el buffer esta vacio
	if (un_buffer->size == 0)
	{
		//reservamos memoria para su size y el int
		un_buffer->stream = malloc(sizeof(int) + tamanio);
		//copiamos en el buffer lo que ingreso
		memcpy(un_buffer->stream, &tamanio, sizeof(int));
		//nos desplazamos y copiamos en el buffer el tamanio de lo que ingreso
		memcpy(un_buffer->stream + sizeof(int), valor, tamanio);
	}
	else{ //sino estaba vacio
		//agreandamos el espacio en memoria a lo que necesitemos
		un_buffer->stream = realloc(un_buffer->stream, un_buffer->size + tamanio + sizeof(int));
		//copiamos en el buffer lo que ingreso
		memcpy(un_buffer->stream + un_buffer->size, &tamanio, sizeof(int));
		//nos desplazamos y copiamos en el buffer el tamanio de lo que ingreso
		memcpy(un_buffer->stream + un_buffer->size + sizeof(int), valor, tamanio);
	}
	

	//actualizamos el buffer
	un_buffer->size += sizeof(int);
	un_buffer->size += tamanio;
	//un_buffer->size += tamanio + sizeof(int);
}

void eliminar_buffer(t_buffer* un_buffer)
{
	if (un_buffer != NULL)
	{
		free(un_buffer->stream);
	}
	free(un_buffer);
}

void* extraer_contenido_del_buffer(t_buffer* un_buffer)
{
	if (un_buffer->size == 0)
	{
		printf("\n[ERROR] Al intentar extrar contenido de un t_buffer vacio \n\n");
		exit(EXIT_FAILURE);

	}
	
	if (un_buffer->size < 0)
	{
		printf("\n[ERROR] Esto es raro. El t_buffer contiene un size negativo \n\n");
		exit(EXIT_FAILURE);
	}
	
	int tamanio_contenido;
	memcpy(&tamanio_contenido, un_buffer->stream, sizeof(int));
	void* contenido = malloc(tamanio_contenido);
	memcpy(contenido, un_buffer->stream + sizeof(int), tamanio_contenido);

	int nuevo_tamanio = un_buffer->size - sizeof(int) - tamanio_contenido;


	if (nuevo_tamanio == 0)
	{
		un_buffer->size = 0;
		free(un_buffer->stream);
		un_buffer->stream = NULL;
		return contenido;
	}
	
	if (nuevo_tamanio < 0)
	{
		perror("\n[ERROR] Buffer con tamanio negativo");
		exit(EXIT_FAILURE);
	}
	
	void* nuevo_stream = malloc(nuevo_tamanio);
	memcpy(nuevo_stream, un_buffer->stream + sizeof(int) + tamanio_contenido, nuevo_tamanio);
	free(un_buffer->stream);
	un_buffer->size = nuevo_tamanio;
	un_buffer->stream = nuevo_stream;

	return contenido;

}


char* extraer_string_del_buffer(t_buffer* un_buffer)
{
	char* valor_string = extraer_contenido_del_buffer(un_buffer);
	return valor_string;
}

//Agrega una variable de tipo int al buffer
void cargar_int_al_buffer(t_buffer* un_buffer, int tamanio_int)
{
	agregar_a_buffer(un_buffer, &tamanio_int, sizeof(int));
}

//Agrega una variable de tipo uint32 al buffer
void cargar_uint32_al_buffer(t_buffer* un_buffer, uint32_t tamanio_uint32)
{
	agregar_a_buffer(un_buffer, &tamanio_uint32, sizeof(uint32_t));
}

//Agrega una variable de tipo string al buffer
void cargar_string_al_buffer(t_buffer* un_buffer, char* tamanio_string)
{
	agregar_a_buffer(un_buffer, tamanio_string, strlen(tamanio_string) + 1);

}
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}


void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);
	
	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}



bool config_has_all_properties(t_config *cfg, char **properties)
{
    for (uint8_t i = 0; properties[i] != NULL; i++)
    {
        if (!config_has_property(cfg, properties[i]))
            return false;
    }
    return true;
}

t_config* iniciar_config(char* path_config, t_log* logger) {
    t_config* nuevo_config;
    if((nuevo_config = config_create(path_config)) == NULL){ //config_create: Devuelve un puntero hacia la estructura creada o NULL en caso de no encontrar el archivo en el path especificado
        printf("No pude leer la config"); // Usar loggs
        log_error(logger, "No se pudo leer la config :(");
		//exit(2); // Por qué 2???
		exit(EXIT_FAILURE);
    }
    return nuevo_config;

}

void imprimir_stream(void* stream, int size) {
    unsigned char* bytes = (unsigned char*)stream;
    for (int i = 0; i < size; i++) {
        printf("%02x ", bytes[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

t_tipo_interfaz_enum obtener_tipo_interfaz_enum (const char* tipo_interfaz_str) {
    if (strcmp(tipo_interfaz_str, "GENERICA") == 0) {
        return GENERICA ;
    } else if (strcmp(tipo_interfaz_str, "STDIN") == 0) {
        return STDIN ;
    } else if (strcmp(tipo_interfaz_str, "STDOUT") == 0) {
        return STDOUT ;
    } else if (strcmp(tipo_interfaz_str, "DIALFS") == 0) {
        return DIALFS ;    
    } else {
        // Manejo de error para tipos de interfaz desconocidos
        return -1; 
    }
}

// Kernel recibe una interfaz con op_cod = INTERFAZ_ENVIAR
 t_interfaz* deserializar_interfaz(t_list*  lista_paquete ){

    t_interfaz* interfaz = malloc(sizeof(t_interfaz));

    interfaz->nombre_length = *(uint32_t*)list_get(lista_paquete, 0);
    interfaz->nombre = list_get(lista_paquete, 1);
    interfaz->tipo = *(uint32_t*)list_get(lista_paquete, 2);
    
	return interfaz;
}

// kernel envia el tiempo a esperar a IO
void enviar_espera(t_io_espera* io_espera, int socket){

    t_paquete* paquete_espera = malloc(sizeof(t_paquete)); 

    paquete_espera = crear_paquete(IO_K_GEN_SLEEP); 
    agregar_a_paquete(paquete_espera, &io_espera->pid, sizeof(io_espera->pid));  
    agregar_a_paquete(paquete_espera, &io_espera->tiempo_espera, sizeof(io_espera->tiempo_espera));      
       
    enviar_paquete(paquete_espera, socket);  

}



//Memoria deserializa lo enviado de Kernel
t_m_crear_proceso* deserializar_crear_proceso(t_list*  lista_paquete ){

    //Creamos una variable de tipo struct que ira guardando todo del paquete y le asignamos tamaño
    t_m_crear_proceso* crear_proceso = malloc(sizeof(t_m_crear_proceso));
    
    crear_proceso->pid = *(uint32_t*)list_get(lista_paquete, 0);
    printf("Pid recibido: %d \n", crear_proceso->pid);

    crear_proceso->archivo_pseudocodigo = list_get(lista_paquete, 1);
    printf("Nombre del proceso: %s \n", crear_proceso->archivo_pseudocodigo);

    return crear_proceso;

}

//Memoria envia proceso creado a Kernel
void enviar_respuesta_crear_proceso(t_m_crear_proceso* crear_proceso ,int socket_kernel) {
    t_paquete* paquete_crear_proceso;
 
    paquete_crear_proceso = crear_paquete(CREAR_PROCESO_KERNEL_FIN);
 
    agregar_a_paquete(paquete_crear_proceso, &crear_proceso->pid,  sizeof(uint32_t));
    agregar_a_paquete(paquete_crear_proceso, crear_proceso->archivo_pseudocodigo, strlen(crear_proceso->archivo_pseudocodigo) + 1);  
    
    enviar_paquete(paquete_crear_proceso, socket_kernel);   
    printf("Proceso enviado: %s\n", crear_proceso->archivo_pseudocodigo); 
    free(paquete_crear_proceso);
    
}



uint32_t* deserializar_finalizar_proceso(t_list*  lista_paquete ){

    //Creamos una variable de tipo struct que ira guardando todo del paquete y le asignamos tamaño
    uint32_t* proceso_a_finalizar = malloc(sizeof(uint32_t));
    
    proceso_a_finalizar = *(uint32_t*)list_get(lista_paquete, 0);
    printf("Pid recibido: %d \n", proceso_a_finalizar);

    return proceso_a_finalizar;
}


void enviar_respuesta_finalizar_proceso(uint32_t pid_proceso_a_finalizar ,int socket_kernel) {
    t_paquete* paquete_finalizar_proceso;
 
    paquete_finalizar_proceso = crear_paquete(FINALIZAR_PROCESO_FIN);
 
    agregar_a_paquete(paquete_finalizar_proceso, &pid_proceso_a_finalizar,  sizeof(uint32_t));
    
    enviar_paquete(paquete_finalizar_proceso, socket_kernel);   
    printf("Proceso enviado"); 
    free(paquete_finalizar_proceso);
    
}


//Mmoria deserializa lo enviado por Cpu
t_proceso_memoria* deserializar_proxima_instruccion(t_list*  lista_paquete ){

    t_proceso_memoria* proxima_instruccion = malloc(sizeof(t_proceso_memoria));
    
    proxima_instruccion->pid = *(uint32_t*)list_get(lista_paquete, 0);
    printf("Pid recibido: %d \n", proxima_instruccion->pid);
    
    proxima_instruccion->program_counter = *(uint32_t*)list_get(lista_paquete, 1);
    printf("Program counter: %d \n", proxima_instruccion->program_counter);

    
    return proxima_instruccion;

}



t_busqueda_marco* deserializar_solicitud_marco(t_list*  lista_paquete ){

    t_busqueda_marco* solicitud_pagina = malloc(sizeof(t_busqueda_marco));
    
    solicitud_pagina->pid = *(uint32_t*)list_get(lista_paquete, 0);
    printf("Pid recibido: %d \n", solicitud_pagina->pid);
    
    solicitud_pagina->nro_pagina = *(uint32_t*)list_get(lista_paquete, 1);
    printf("Numero de pagina: %d \n", solicitud_pagina->nro_pagina);

    
    return solicitud_pagina;

}



t_io_direcciones_fisicas* deserializar_peticion_valor(t_list*  lista_paquete ){

    t_io_direcciones_fisicas* peticion_valor = malloc(sizeof(t_io_direcciones_fisicas));
    
    peticion_valor->pid = *(uint32_t*)list_get(lista_paquete, 0);
    printf("Pid recibido: %d \n", peticion_valor->pid);
    
    uint32_t tamanio_lista = *(uint32_t*)list_get(lista_paquete, 1);
    printf("tamanio lista: %d \n",tamanio_lista);

    // Deserializar cada elemento de la lista
    peticion_valor->direcciones_fisicas = list_create();

    for (int i = 0; i < tamanio_lista; i++) {

        uint32_t* direccion_fisica = malloc(sizeof(uint32_t));
        direccion_fisica = *(uint32_t*)list_get(lista_paquete, 2 + i);
        printf("Posicion %d, valor %ls \n",2 + i, direccion_fisica) ;
        list_add(peticion_valor->direcciones_fisicas, direccion_fisica);
        printf("Valor agregado %ls \n",direccion_fisica);
    }

    
    return peticion_valor;

}




t_escribir_leer* deserializar_peticion_guardar(t_list*  lista_paquete){

    //Creamos una variable de tipo struct que ira guardando todo del paquete y le asignamos tamaño
    t_escribir_leer* peticion_guardar = malloc(sizeof(t_escribir_leer));
    
    peticion_guardar->pid = *(uint32_t*)list_get(lista_paquete, 0);
    printf("Pid recibido: %d \n", peticion_guardar->pid);
    
    peticion_guardar->direccion_fisica = *(uint32_t*)list_get(lista_paquete, 1);
    printf("Direccion fisica: %d \n", peticion_guardar->direccion_fisica);

    peticion_guardar->tamanio = *(uint32_t*)list_get(lista_paquete, 2);
    printf("Tamanio proceso: %d \n", peticion_guardar->tamanio);

    peticion_guardar->valor = list_get(lista_paquete, 3);
    printf("Valor: %s \n", peticion_guardar->valor);

    return peticion_guardar;

}



t_resize* deserializar_solicitud_resize(t_list*  lista_paquete){

    //Creamos una variable de tipo struct que ira guardando todo del paquete y le asignamos tamaño
    t_resize* solicitud_resize = malloc(sizeof(t_resize));
    
    solicitud_resize->pid = *(uint32_t*)list_get(lista_paquete, 0);
    printf("Pid recibido: %d \n", solicitud_resize->pid);
    
    solicitud_resize->tamanio = *(uint32_t*)list_get(lista_paquete, 1);
    printf("Tamanio proceso: %d \n", solicitud_resize->tamanio);

    solicitud_resize->valor = list_get(lista_paquete, 2);
    printf("Valor: %s \n", solicitud_resize->valor);

    return solicitud_resize;

}


t_copy* deserializar_solicitud_copy(t_list*  lista_paquete){

    //Creamos una variable de tipo struct que ira guardando todo del paquete y le asignamos tamaño
    t_copy* solicitud_copy = malloc(sizeof(t_copy));
    
    solicitud_copy->pid = *(uint32_t*)list_get(lista_paquete, 0);
    printf("Pid recibido: %d \n", solicitud_copy->pid);
    
    solicitud_copy->direccion_fisica = *(uint32_t*)list_get(lista_paquete, 1);
    printf("Direccion fisica: %d \n", solicitud_copy->direccion_fisica);

    solicitud_copy->valor = list_get(lista_paquete, 2);
    printf("Valor: %s \n", solicitud_copy->valor);

    return solicitud_copy;

}


void enviar_respuesta_instruccion(char* proxima_instruccion ,int socket_cpu) {
    t_paquete* paquete_instruccion;
 
    paquete_instruccion = crear_paquete(INSTRUCCION_RECIBIDA);
 
    agregar_a_paquete(paquete_instruccion, &proxima_instruccion,  strlen(proxima_instruccion) + 1);          
    
    enviar_paquete(paquete_instruccion, socket_cpu);   
    printf("Instruccion enviada"); 
    free(paquete_instruccion);
    
}



void enviar_solicitud_marco(int marco ,int socket_cpu) {
    t_paquete* paquete_marco;
 
    paquete_marco = crear_paquete(MARCO_RECIBIDO);
 
    agregar_a_paquete(paquete_marco, &marco,  sizeof(int));         
    
    enviar_paquete(paquete_marco, socket_cpu);   
    printf("Marco enviado"); 
    free(paquete_marco);
    
}

void enviar_solicitud_tamanio(uint32_t tamanio_pagina ,int socket_cpu) {
    t_paquete* paquete_tam_pagina;
 
    paquete_tam_pagina = crear_paquete(SOLICITUD_TAMANIO_PAGINA_RTA);
 
    agregar_a_paquete(paquete_tam_pagina, &tamanio_pagina,  sizeof(uint32_t));         
    
    enviar_paquete(paquete_tam_pagina, socket_cpu);   
    printf("Tamaño de pagina enviada"); 
    free(paquete_tam_pagina);
    
}




void enviar_peticion_valor(void* respuesta_leer ,int socket_cpu) {
    t_paquete* paquete_valor;
 
    paquete_valor = crear_paquete(PETICION_VALOR_MEMORIA_RTA);
 
    agregar_a_paquete(paquete_valor, &respuesta_leer,  sizeof(void*));          
    
    enviar_paquete(paquete_valor, socket_cpu);   
    printf("Se envio respuesta de lectura"); 
    free(paquete_valor);
    
}



void enviar_resultado_guardar(void* respuesta_escribir, int socket_cliente){
    t_paquete* paquete_valor;

    paquete_valor = crear_paquete(GUARDAR_EN_DIRECCION_FISICA_RTA);

    agregar_a_paquete(paquete_valor, &respuesta_escribir,  sizeof(void*));

    enviar_paquete(paquete_valor, socket_cliente);
    printf("Se envio respuesta de guardado"); 
    free(paquete_valor);
}


void enviar_respuesta_resize(op_code respuesta_resize, int socket_cliente){
    t_paquete* paquete_valor;

    paquete_valor = crear_paquete(SOLICITUD_RESIZE_RTA);

    agregar_a_paquete(paquete_valor, &respuesta_resize,  sizeof(void*));

    enviar_paquete(paquete_valor, socket_cliente);
    printf("Se envio respuesta de resize"); 
    free(paquete_valor);
}


void enviar_resultado_copiar(void* respuesta_copy, int socket_cliente){
    t_paquete* paquete_valor;

    paquete_valor = crear_paquete(ENVIO_COPY_STRING_A_MEMORIA_RTA);

    agregar_a_paquete(paquete_valor, &respuesta_copy,  sizeof(void*));

    enviar_paquete(paquete_valor, socket_cliente);
    printf("Se envio respuesta de copiado"); 
    free(paquete_valor);
}



// usar en memoria cuando recibe IO_M_STDIN
t_io_memo_escritura* deserializar_input(t_list*  lista_paquete ){

    t_io_memo_escritura* io_input = malloc(sizeof(t_io_memo_escritura));
    
    io_input->pid = *(uint32_t*)list_get(lista_paquete, 0);
    printf("Pid recibido: %d \n",io_input->pid);
    
    uint32_t tamanio_lista = *(uint32_t*)list_get(lista_paquete, 1);
    printf("tamanio lista: %d \n",tamanio_lista);

     // Deserializar cada elemento de la lista
    io_input->direcciones_fisicas = list_create();
    for (int i = 0; i < tamanio_lista; i++) {
        uint32_t* direccion_fisica = malloc(sizeof(uint32_t));
        direccion_fisica = *(uint32_t*)list_get(lista_paquete, 2 + i);
        printf("Posicion %d, valor %d \n",2 + i, direccion_fisica) ;
        list_add(io_input->direcciones_fisicas, direccion_fisica);
         printf("Valor agregado %d \n",direccion_fisica);
    }

    io_input->input_length = *(uint32_t*)list_get(lista_paquete,2+tamanio_lista);
    printf("Cantidad caracteres input: %d \n",io_input->input_length);
    io_input->input = list_get(lista_paquete, 2+tamanio_lista+1);
    printf("Input: %s \n",io_input->input);    

    return io_input;
}

// Kernel envía a io un stdin usando op_cod= IO_K_STDIN
void enviar_io_df(t_io_direcciones_fisicas* io_df, int socket, op_code codigo_operacion){

    t_paquete* paquete_df = malloc(sizeof(t_paquete));    
   
    paquete_df = crear_paquete(codigo_operacion); 
    
    agregar_a_paquete(paquete_df, &io_df->pid, sizeof(io_df->pid)); 
    
    uint32_t list_tamanio = list_size(io_df->direcciones_fisicas);
   
    agregar_a_paquete(paquete_df, &list_tamanio, sizeof(uint32_t));

    for (int i = 0; i < list_tamanio; i++) {
        uint32_t* direccion_fisica = (uint32_t*) list_get(io_df->direcciones_fisicas, i);
        
        agregar_a_paquete(paquete_df,  &direccion_fisica, sizeof(uint32_t));
        
    }   
     agregar_a_paquete(paquete_df, &io_df->tamanio_operacion, sizeof(uint32_t));    
    enviar_paquete(paquete_df, socket);
     free(paquete_df);  
  printf("Se envio io df\n");

}

 t_io_direcciones_fisicas* deserializar_io_df(t_list*  lista_paquete ){

    t_io_direcciones_fisicas* io_df = malloc(sizeof(t_io_direcciones_fisicas));
    io_df->pid = *(uint32_t*)list_get(lista_paquete, 0);
    uint32_t tamanio_lista = *(uint32_t*)list_get(lista_paquete, 1);
   
  // Deserializar cada elemento de la lista
    io_df->direcciones_fisicas = list_create();
    for (int i = 0; i < tamanio_lista; i++) {
        uint32_t* direccion_fisica = malloc(sizeof(uint32_t));
        direccion_fisica = *(uint32_t*)list_get(lista_paquete, 2 + i);
        list_add(io_df->direcciones_fisicas, direccion_fisica);
    }
    io_df->tamanio_operacion = *(uint32_t*)list_get(lista_paquete,2+tamanio_lista);
    printf("Tamanio operacion: %d \n",io_df->tamanio_operacion);
    return io_df;

}

//Memoria envia a entradasalida
void enviar_output(t_io_output* io_output ,int socket_io, uint32_t op_code) {
    t_paquete* paquete_output;
 
    paquete_output = crear_paquete(op_code);
 
    agregar_a_paquete(paquete_output,  &io_output->pid,  sizeof(uint32_t));         
    agregar_a_paquete(paquete_output, &io_output->output_length, sizeof(uint32_t));  
    agregar_a_paquete(paquete_output, io_output->output, io_output->output_length);  
    
    enviar_paquete(paquete_output, socket_io);   
    printf("Output enviado: %s\n",io_output->output) ; 
    free(paquete_output);
    
}

// Entradasalida  recibe de memoria
 t_io_output* deserializar_output(t_list*  lista_paquete ){

    t_io_output* io_output = malloc(sizeof(t_io_output));
    io_output->pid = *(uint32_t*)list_get(lista_paquete, 0);
    io_output->output_length = *(uint32_t*)list_get(lista_paquete, 1);
    io_output->output = list_get(lista_paquete, 2);  
    
	return io_output;
}

void  enviar_gestionar_archivo(t_io_gestion_archivo* nuevo_archivo, int socket, uint32_t cod_op ){
    t_paquete* paquete_archivo_nuevo = malloc(sizeof(t_paquete));;
    
    paquete_archivo_nuevo = crear_paquete(cod_op);
    
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->pid), sizeof(nuevo_archivo->pid));
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->nombre_archivo_length, sizeof(nuevo_archivo->nombre_archivo_length));  
    agregar_a_paquete(paquete_archivo_nuevo, nuevo_archivo->nombre_archivo, nuevo_archivo->nombre_archivo_length);
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->tamanio_archivo), sizeof(nuevo_archivo->tamanio_archivo));   
    enviar_paquete(paquete_archivo_nuevo, socket);    
}

void enviar_input(t_io_memo_escritura* io_input ,int socket, uint32_t op_code ) {
    t_paquete* paquete_input;
 
    paquete_input = crear_paquete(op_code);
 
    agregar_a_paquete(paquete_input,  &io_input->pid,  sizeof(uint32_t));      
    uint32_t list_tamanio = list_size(io_input->direcciones_fisicas);    
    agregar_a_paquete(paquete_input, &list_tamanio, sizeof(uint32_t));  
    //agrego cada elemento de la lista de direcciones fisicas
    for (int i = 0; i < list_tamanio; i++) {
        uint32_t direccion_fisica = (uint32_t*) list_get(io_input->direcciones_fisicas, i);        
        agregar_a_paquete(paquete_input,  &direccion_fisica, sizeof(uint32_t));        
    }   

    agregar_a_paquete(paquete_input, &io_input->input_length, sizeof(uint32_t));  
    agregar_a_paquete(paquete_input, io_input->input, io_input->input_length);  
    enviar_paquete(paquete_input, socket);    
    free(paquete_input);     

}

void enviar_io_readwrite(t_io_readwrite_archivo* io_readwrite ,int socket, uint32_t op_code ){
    t_paquete* paquete_readwrite;
    uint32_t list_tamanio = list_size(io_readwrite->direcciones_fisicas); 
    paquete_readwrite = crear_paquete(op_code);
 
    agregar_a_paquete(paquete_readwrite,  &io_readwrite->pid,  sizeof(uint32_t));     
    agregar_a_paquete(paquete_readwrite, &io_readwrite->nombre_archivo_length, sizeof(uint32_t));  
    agregar_a_paquete(paquete_readwrite, io_readwrite->nombre_archivo, io_readwrite->nombre_archivo_length);       
    agregar_a_paquete(paquete_readwrite, &list_tamanio, sizeof(uint32_t));  
    //agrego cada elemento de la lista de direcciones fisicas
    for (int i = 0; i < list_tamanio; i++) {
        uint32_t direccion_fisica = (uint32_t*) list_get(io_readwrite->direcciones_fisicas, i);        
        agregar_a_paquete(paquete_readwrite,  &direccion_fisica, sizeof(uint32_t));        
    }   
    agregar_a_paquete(paquete_readwrite,  &io_readwrite->tamanio_operacion,  sizeof(uint32_t));  
    agregar_a_paquete(paquete_readwrite,  &io_readwrite->puntero_archivo,  sizeof(uint32_t));   
    enviar_paquete(paquete_readwrite, socket);    
    free(paquete_readwrite); 
}

t_io_readwrite_archivo* deserializar_io_readwrite(t_list*  lista_paquete ){

    t_io_readwrite_archivo* io_readwrite = malloc(sizeof(t_io_readwrite_archivo));
    
    io_readwrite->pid = *(uint32_t*)list_get(lista_paquete, 0);
    io_readwrite->nombre_archivo_length = *(uint32_t*)list_get(lista_paquete,1);
    io_readwrite->nombre_archivo = list_get(lista_paquete, 2);
    printf("Nombre archivo: %s \n",io_readwrite->nombre_archivo); 

    uint32_t tamanio_lista = *(uint32_t*)list_get(lista_paquete, 3);
    printf("tamanio lista: %d \n",tamanio_lista); // despues borrar print

     // Deserializar cada elemento de la lista
    io_readwrite->direcciones_fisicas = list_create();
    for (int i = 0; i < tamanio_lista; i++) {
        uint32_t* direccion_fisica = malloc(sizeof(uint32_t));
        direccion_fisica = *(uint32_t*)list_get(lista_paquete, 4 + i);
        printf("Posicion %d, valor %d \n",4 + i, direccion_fisica) ; // despues borrar print
        list_add(io_readwrite->direcciones_fisicas, direccion_fisica);
         printf("Valor agregado %d \n",direccion_fisica); // despues borrar print
    }
    io_readwrite->tamanio_operacion = *(uint32_t*)list_get(lista_paquete,4+tamanio_lista);
     printf("tamanio operacion %d \n",io_readwrite->tamanio_operacion);// despues borrar print
    io_readwrite->puntero_archivo = *(uint32_t*)list_get(lista_paquete,4+tamanio_lista+1);
     printf("Puntero archivo %d \n",io_readwrite->puntero_archivo); // despues borrar print
    return io_readwrite; 
    free(io_readwrite);
}

t_io_output* armar_io_output(uint32_t pid, char* output){
            t_io_output* io_output = malloc(sizeof(t_io_output));
            uint32_t tamanio_output = string_length(output)+1;
            io_output->pid = pid;
            io_output->output_length = tamanio_output;
            io_output->output = output;
           return  io_output;    
}

void terminar_programa(int conexion, t_log* logger, t_config* config)
{
	  log_destroy(logger);
	  config_destroy(config);
	  liberar_conexion(conexion);

}

 t_io_crear_archivo* deserializar_io_crear_archivo(t_list*  lista_paquete ){

    t_io_crear_archivo* io_crear_archivo = malloc(sizeof(t_io_crear_archivo));
    io_crear_archivo->pid = *(uint32_t*)list_get(lista_paquete, 0);
    io_crear_archivo->nombre_archivo_length = *(uint32_t*)list_get(lista_paquete, 1);
    io_crear_archivo->nombre_archivo = list_get(lista_paquete, 2);
    io_crear_archivo->interfaz->nombre_length = *(uint32_t*)list_get(lista_paquete, 3); //AGREGADO
    io_crear_archivo->interfaz->nombre = list_get(lista_paquete, 4); //AGREGADO
    io_crear_archivo->interfaz->tipo = *(uint32_t*)list_get(lista_paquete, 5); //AGREGADO

	return io_crear_archivo;
}

void  enviar_creacion_archivo(t_io_crear_archivo* nuevo_archivo, int socket ){
    t_paquete* paquete_archivo_nuevo = malloc(sizeof(t_paquete));
    
    paquete_archivo_nuevo = crear_paquete(IO_FS_CREATE);
    
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->pid), sizeof(nuevo_archivo->pid));
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->nombre_archivo_length, sizeof(nuevo_archivo->nombre_archivo_length));  
    agregar_a_paquete(paquete_archivo_nuevo, nuevo_archivo->nombre_archivo, nuevo_archivo->nombre_archivo_length);
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->interfaz->nombre_length, sizeof(nuevo_archivo->interfaz->nombre_length)); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, nuevo_archivo->interfaz->nombre, nuevo_archivo->interfaz->nombre_length); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->interfaz->tipo, sizeof(nuevo_archivo->interfaz->tipo)); //AGREGADO
    enviar_paquete(paquete_archivo_nuevo, socket);  
    
}

void  enviar_delete_archivo(t_io_crear_archivo* nuevo_archivo, int socket ){
    t_paquete* paquete_archivo_nuevo = malloc(sizeof(t_paquete));
    
    paquete_archivo_nuevo = crear_paquete(IO_FS_DELETE);
    
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->pid), sizeof(nuevo_archivo->pid));
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->nombre_archivo_length, sizeof(nuevo_archivo->nombre_archivo_length));  
    agregar_a_paquete(paquete_archivo_nuevo, nuevo_archivo->nombre_archivo, nuevo_archivo->nombre_archivo_length);
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->interfaz->nombre_length, sizeof(nuevo_archivo->interfaz->nombre_length)); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, nuevo_archivo->interfaz->nombre, nuevo_archivo->interfaz->nombre_length); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->interfaz->tipo, sizeof(nuevo_archivo->interfaz->tipo)); //AGREGADO
    enviar_paquete(paquete_archivo_nuevo, socket);  
    
}

 t_io_fs_truncate* deserializar_io_truncate_archivo(t_list*  lista_paquete ){

    t_io_fs_truncate* io_truncate_archivo = malloc(sizeof(t_io_fs_truncate));
    io_truncate_archivo->pid = *(uint32_t*)list_get(lista_paquete, 0);
    io_truncate_archivo->nombre_archivo_length = *(uint32_t*)list_get(lista_paquete, 1);
    io_truncate_archivo->nombre_archivo = list_get(lista_paquete, 2);
    io_truncate_archivo->interfaz->nombre_length = *(uint32_t*)list_get(lista_paquete, 3); //AGREGADO
    io_truncate_archivo->interfaz->nombre = list_get(lista_paquete, 4); //AGREGADO
    io_truncate_archivo->interfaz->tipo = *(uint32_t*)list_get(lista_paquete, 5); //AGREGADO
    io_truncate_archivo->tamanio = *(uint32_t*)list_get(lista_paquete, 6);

	return io_truncate_archivo;
}

void  enviar_truncate_archivo(t_io_fs_truncate* nuevo_archivo, int socket ){
    t_paquete* paquete_archivo_nuevo = malloc(sizeof(t_paquete));
    
    paquete_archivo_nuevo = crear_paquete(IO_FS_TRUNCATE);
    
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->pid), sizeof(nuevo_archivo->pid));
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->nombre_archivo_length, sizeof(nuevo_archivo->nombre_archivo_length));  
    agregar_a_paquete(paquete_archivo_nuevo, nuevo_archivo->nombre_archivo, nuevo_archivo->nombre_archivo_length);
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->interfaz->nombre_length, sizeof(nuevo_archivo->interfaz->nombre_length)); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, nuevo_archivo->interfaz->nombre, nuevo_archivo->interfaz->nombre_length); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->interfaz->tipo, sizeof(nuevo_archivo->interfaz->tipo)); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->tamanio), sizeof(nuevo_archivo->tamanio));
    enviar_paquete(paquete_archivo_nuevo, socket);  
    
}

 t_io_fs_write* deserializar_io_write_archivo(t_list*  lista_paquete ){

    t_io_fs_write* io_write_archivo = malloc(sizeof(t_io_fs_write));
    io_write_archivo->pid = *(uint32_t*)list_get(lista_paquete, 0);
    io_write_archivo->nombre_archivo_length = *(uint32_t*)list_get(lista_paquete, 1);
    io_write_archivo->nombre_archivo = list_get(lista_paquete, 2);
    io_write_archivo->interfaz->nombre_length = *(uint32_t*)list_get(lista_paquete, 3); //AGREGADO
    io_write_archivo->interfaz->nombre = list_get(lista_paquete, 4); //AGREGADO
    io_write_archivo->interfaz->tipo = *(uint32_t*)list_get(lista_paquete, 5); //AGREGADO
    io_write_archivo->direccion= *(uint32_t*)list_get(lista_paquete, 6);
    io_write_archivo->tamanio = *(uint32_t*)list_get(lista_paquete, 7);
    io_write_archivo->puntero_archivo = *(uint32_t*)list_get(lista_paquete, 8);
    

	return io_write_archivo;
}

void  enviar_write_archivo(t_io_fs_write* nuevo_archivo, int socket ){
    t_paquete* paquete_archivo_nuevo = malloc(sizeof(t_paquete));
    
    paquete_archivo_nuevo = crear_paquete(IO_FS_WRITE);
    
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->pid), sizeof(nuevo_archivo->pid));
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->nombre_archivo_length, sizeof(nuevo_archivo->nombre_archivo_length));  
    agregar_a_paquete(paquete_archivo_nuevo, nuevo_archivo->nombre_archivo, nuevo_archivo->nombre_archivo_length);
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->interfaz->nombre_length, sizeof(nuevo_archivo->interfaz->nombre_length)); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, nuevo_archivo->interfaz->nombre, nuevo_archivo->interfaz->nombre_length); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->interfaz->tipo, sizeof(nuevo_archivo->interfaz->tipo)); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->direccion), sizeof(nuevo_archivo->direccion));
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->tamanio), sizeof(nuevo_archivo->tamanio));
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->puntero_archivo), sizeof(nuevo_archivo->puntero_archivo));
    enviar_paquete(paquete_archivo_nuevo, socket);  
    
}


void  enviar_read_archivo(t_io_fs_write* nuevo_archivo, int socket ){
    t_paquete* paquete_archivo_nuevo = malloc(sizeof(t_paquete));
    
    paquete_archivo_nuevo = crear_paquete(IO_FS_READ);
    
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->pid), sizeof(nuevo_archivo->pid));
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->nombre_archivo_length, sizeof(nuevo_archivo->nombre_archivo_length));  
    agregar_a_paquete(paquete_archivo_nuevo, nuevo_archivo->nombre_archivo, nuevo_archivo->nombre_archivo_length);
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->interfaz->nombre_length, sizeof(nuevo_archivo->interfaz->nombre_length)); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, nuevo_archivo->interfaz->nombre, nuevo_archivo->interfaz->nombre_length); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, &nuevo_archivo->interfaz->tipo, sizeof(nuevo_archivo->interfaz->tipo)); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->direccion), sizeof(nuevo_archivo->direccion));
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->tamanio), sizeof(nuevo_archivo->tamanio));
    agregar_a_paquete(paquete_archivo_nuevo, &(nuevo_archivo->puntero_archivo), sizeof(nuevo_archivo->puntero_archivo));
    enviar_paquete(paquete_archivo_nuevo, socket);  
    
}

 t_pcb* deserializar_pcb(t_list*  lista_paquete ){

    t_pcb* pcb = malloc(sizeof(t_pcb));
    pcb->pid = *(uint32_t*)list_get(lista_paquete, 0);
    pcb->program_counter = *(uint32_t*)list_get(lista_paquete, 1);
    pcb->path_length = *(uint32_t*)list_get(lista_paquete, 2);
    pcb->path = list_get(lista_paquete, 3);
    //TODO: VER EL TEMA DEL CAMPO LISTA_RECURSOS_PCB
    /*uint32_t tamanio_lista = *(uint32_t*)list_get(lista_paquete, 4);

     // Deserializar cada elemento de la lista
    pcb->lista_recursos_pcb = list_create();
    for (int i = 0; i < tamanio_lista; i++) {
        uint32_t* recurso = malloc(sizeof(uint32_t)); //de que tipo son los objetos de la lista?
        recurso = *(uint32_t*)list_get(lista_paquete, 2 + i);
        list_add(pcb->lista_recursos_pcb, recurso);
    }*/

    /*io_write_archivo->pid = *(uint32_t*)list_get(lista_paquete, 0);
    io_write_archivo->nombre_archivo_length = *(uint32_t*)list_get(lista_paquete, 1);
    io_write_archivo->nombre_archivo = list_get(lista_paquete, 2);
    io_write_archivo->interfaz->nombre_length = *(uint32_t*)list_get(lista_paquete, 3); //AGREGADO
    io_write_archivo->interfaz->nombre = list_get(lista_paquete, 4); //AGREGADO
    io_write_archivo->interfaz->tipo = *(uint32_t*)list_get(lista_paquete, 5); //AGREGADO
    io_write_archivo->direccion= *(uint32_t*)list_get(lista_paquete, 6);
    io_write_archivo->tamanio = *(uint32_t*)list_get(lista_paquete, 7);
    io_write_archivo->puntero_archivo = *(uint32_t*)list_get(lista_paquete, 8);*/

    //pcb->mutex_lista_recursos = *(pthread_mutex_t*)list_get(lista_paquete, 5); //Revisar tamanio
    //pcb->registros_cpu = (t_registros_CPU)list_get(lista_paquete, 6); //VER COMO DESEREALIZAR T_REGISTROS_CPU
    pcb->estado = pcb->path_length = *(uint32_t*)list_get(lista_paquete, 7);
    pcb->tiempo_ejecucion = *(uint32_t*)list_get(lista_paquete, 8);
	return pcb;
}

 t_io_stdin_stdout* deserializar_io_stdin_stdout(t_list*  lista_paquete ){

    t_io_stdin_stdout* io_stdin_stdout = malloc(sizeof(t_io_stdin_stdout));
    io_stdin_stdout->pid = *(uint32_t*)list_get(lista_paquete, 0);
    io_stdin_stdout->interfaz->nombre_length = *(uint32_t*)list_get(lista_paquete, 1); //AGREGADO
    io_stdin_stdout->interfaz->nombre = list_get(lista_paquete, 2); //AGREGADO
    io_stdin_stdout->interfaz->tipo = *(uint32_t*)list_get(lista_paquete, 3); //AGREGADO
    io_stdin_stdout->direccion= *(uint32_t*)list_get(lista_paquete, 4);
    io_stdin_stdout->tamanio = *(uint32_t*)list_get(lista_paquete, 5);
    

	return io_stdin_stdout;
}

void  enviar_io_stdin_read(t_io_stdin_stdout* io_stdin_read, int socket ){
    t_paquete* paquete_archivo_nuevo = malloc(sizeof(t_paquete));
    
    paquete_archivo_nuevo = crear_paquete(IO_K_STDIN);
    
    agregar_a_paquete(paquete_archivo_nuevo, &(io_stdin_read->pid), sizeof(io_stdin_read->pid));
    agregar_a_paquete(paquete_archivo_nuevo, &io_stdin_read->interfaz->nombre_length, sizeof(io_stdin_read->interfaz->nombre_length)); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, io_stdin_read->interfaz->nombre, io_stdin_read->interfaz->nombre_length); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, &io_stdin_read->interfaz->tipo, sizeof(io_stdin_read->interfaz->tipo)); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, &(io_stdin_read->direccion), sizeof(io_stdin_read->direccion));
    agregar_a_paquete(paquete_archivo_nuevo, &(io_stdin_read->tamanio), sizeof(io_stdin_read->tamanio));
    enviar_paquete(paquete_archivo_nuevo, socket);  
    
}

void  enviar_io_stdout_write(t_io_stdin_stdout* io_stdout_write, int socket ){
    t_paquete* paquete_archivo_nuevo = malloc(sizeof(t_paquete));
    
    paquete_archivo_nuevo = crear_paquete(IO_K_STDOUT);
    
    agregar_a_paquete(paquete_archivo_nuevo, &(io_stdout_write->pid), sizeof(io_stdout_write->pid));
    agregar_a_paquete(paquete_archivo_nuevo, &io_stdout_write->interfaz->nombre_length, sizeof(io_stdout_write->interfaz->nombre_length)); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, io_stdout_write->interfaz->nombre, io_stdout_write->interfaz->nombre_length); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, &io_stdout_write->interfaz->tipo, sizeof(io_stdout_write->interfaz->tipo)); //AGREGADO
    agregar_a_paquete(paquete_archivo_nuevo, &(io_stdout_write->direccion), sizeof(io_stdout_write->direccion));
    agregar_a_paquete(paquete_archivo_nuevo, &(io_stdout_write->tamanio), sizeof(io_stdout_write->tamanio));
    enviar_paquete(paquete_archivo_nuevo, socket);  
    
}

 t_io_gen_sleep* deserializar_io_gen_sleep(t_list*  lista_paquete ){

    t_io_gen_sleep* io_gen_sleep = malloc(sizeof(t_io_gen_sleep));
    io_gen_sleep->pid = *(uint32_t*)list_get(lista_paquete, 0);
    io_gen_sleep->interfaz->nombre_length = *(uint32_t*)list_get(lista_paquete, 1); 
    io_gen_sleep->interfaz->nombre = list_get(lista_paquete, 2); 
    io_gen_sleep->interfaz->tipo = *(t_tipo_interfaz_enum*)list_get(lista_paquete, 3); 
    io_gen_sleep->unidades_de_trabajo = *(uint32_t*)list_get(lista_paquete, 4); 
    
	return io_gen_sleep;
}

void  enviar_io_gen_sleep(t_io_gen_sleep* io_gen_sleep, int socket ){
    t_paquete* paquete_archivo_nuevo = malloc(sizeof(t_paquete));
    
    paquete_archivo_nuevo = crear_paquete(IO_K_GEN_SLEEP);
    
    agregar_a_paquete(paquete_archivo_nuevo, &(io_gen_sleep->pid), sizeof(io_gen_sleep->pid));
    agregar_a_paquete(paquete_archivo_nuevo, &io_gen_sleep->interfaz->nombre_length, sizeof(io_gen_sleep->interfaz->nombre_length)); 
    agregar_a_paquete(paquete_archivo_nuevo, io_gen_sleep->interfaz->nombre, io_gen_sleep->interfaz->nombre_length);
    agregar_a_paquete(paquete_archivo_nuevo, &io_gen_sleep->interfaz->tipo, sizeof(io_gen_sleep->interfaz->tipo)); 
    agregar_a_paquete(paquete_archivo_nuevo, &io_gen_sleep->unidades_de_trabajo, sizeof(io_gen_sleep->unidades_de_trabajo)); 

    enviar_paquete(paquete_archivo_nuevo, socket);  
    
}

 t_proceso_interrumpido* deserializar_proceso_interrumpido(t_list*  lista_paquete ){

    t_proceso_interrumpido* proceso_interrumpido = malloc(sizeof(t_proceso_interrumpido));
    proceso_interrumpido->pcb = deserializar_pcb(lista_paquete);
    proceso_interrumpido->tamanio_motivo_interrupcion = *(uint32_t*)list_get(lista_paquete, 9); //ver posicion en la lista
    proceso_interrumpido->motivo_interrupcion = list_get(lista_paquete, 10); //ver posicion en la lista

	return proceso_interrumpido;
}