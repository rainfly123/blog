 #include <stdio.h> 
 #include <stdlib.h>

typedef  struct node
{
    void * data;
    struct node * next;
}qElement;

static qElement *rear = NULL;
static qElement *front = NULL;

void * pop(void)
{
    qElement * var = front;
    void * data = NULL;

    if (var != NULL)
    {
        front = front->next;
        data = var->data;
        free(var);
        if (front == NULL)
            rear = NULL; //very important
    }
    else {
        printf("\nQueue Empty");
    }
    return data;
}

void push(void *value)
{
     qElement *temp;
     temp = (qElement*)malloc(sizeof(qElement));
     temp->data = value;

     if (rear == NULL)
     {
           rear = temp;
           front = rear;
           rear->next = NULL;
     }
     else
     {
           rear->next = temp;
           rear = temp;
           rear->next = NULL;
     }
}

void display()
{
    qElement * var = front;
    if (var != NULL)
    {
        printf("\nElements are as:  ");
        while (var != NULL)
        {
            printf("\t%d", *((int *)var->data));
            var = var->next;
        }
        printf("\n");
    } 
    else
        printf("\nQueue is Empty");
}

int main()
{
     int a = 0;
     int b = 1;
     int c = 2;
     int d = 3;
     int * e ;
     push(&a);
     push(&b);
     push(&c);
     push(&d);
     display();
     e = pop();
     printf("%d\n", *e);
     display();
     e = pop();
     printf("%d\n", *e);
     display();
     e = pop();
     printf("%d\n", *e);
     display();
     e = pop();
     printf("%d\n", *e);
     display();
     e = pop();
     push(&d);
     push(&c);
     push(&b);
     push(&a);
     display();
     e = pop();
     printf("%d\n", *e);
     display();
}
