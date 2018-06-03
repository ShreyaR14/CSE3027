#define BUFF_SIZE 1024
#define MAX_OBJECT_SIZE 1024 * 512
#define MAX_CACHE_SIZE 1024 * 1024 * 5

typedef struct LRU_LinkedList
{
  struct Node *header;
  struct Node *tail;
  int size;
  int count;
} LRU_LinkedList;

typedef struct Node
{
  struct Node *next;
  struct Node *prev;
  int data_size;
  char url[BUFF_SIZE];
  char object[MAX_OBJECT_SIZE];
} Node;

void init_LRU_LinkedList(LRU_LinkedList *list)
{
  list->header = NULL;
  list->tail = NULL;
  list->size = 0;
  list->count = 0;
}

Node *create_node(char *url, char *object)
{
  Node *node = malloc(sizeof(Node));
  node->next = NULL;
  node->prev = NULL;
  node->data_size = strlen(object);

  memset(node->url, 0, BUFF_SIZE);
  memset(node->object, 0, MAX_OBJECT_SIZE);
  strcpy(node->url, url);
  strcpy(node->object, object);
  return node;
}

Node *pop_node(LRU_LinkedList *list)
{
  list->count--;
  list->size -= list->header->data_size;

  if (list->count == 0)
  {
    list->header = NULL;
    list->tail = NULL;
  }
  else
  {
    list->header = list->header->next;
    list->header->prev = NULL;
  }
  return list->header;
}

void add_node(LRU_LinkedList *list, Node *node)
{
  if (list->count == 0)
  {
    list->header = node;
    list->tail = node;
  }
  else
  {
    list->tail->next = node;
    node->prev = list->tail;
    node->next = NULL;
    list->tail = node;
  }
  list->count++;
  list->size += node->data_size;
  while (list->size > MAX_CACHE_SIZE)
    pop_node(list);
}

Node *search_node(LRU_LinkedList *list, char *url)
{
  printf("** Start to search Node in Cache **\n");
  Node *cursor = list->header;
  while (cursor != NULL)
  {
    printf("TEST");
    if (strcmp(cursor->url, url) == 0)
    {
      if (list->size > 1)
      {
        if (cursor == list->header)
        {
          list->header = list->header->next;
          list->header->prev = NULL;

          list->tail->next = cursor;
          cursor->next = NULL;
          cursor->prev = list->tail;
          list->tail = cursor;
        }
        else if (cursor != list->tail)
        {
          cursor->prev->next = cursor->next;
          cursor->next->prev = cursor->prev;

          list->tail->next = cursor;
          cursor->next = NULL;
          cursor->prev = list->tail;
          list->tail = cursor;
        }
      }
      return cursor;
    }
    cursor = cursor->next;
  }
  return NULL;
}

void print_node(Node *node)
{
  printf("%s\t%d bytes\n", node->url, node->data_size);
}

void print_cache(LRU_LinkedList *list)
{
  Node *cursor = list->header;
  printf("Cache Size: %d bytes\n", list->size);
  int cnt = 0;
  while (cursor != NULL)
  {
    cnt++;
    printf("no. %d:\t%s\t%d bytes\n", cnt, cursor->url, cursor->data_size);
    cursor = cursor->next;
  }
}