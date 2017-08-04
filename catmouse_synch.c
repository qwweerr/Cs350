#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>

/* 
 * This simple default synchronization mechanism allows only creature at a time to
 * eat.   The globalCatMouseSem is used as a a lock.   We use a semaphore
 * rather than a lock so that this code will work even before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

static volatile int eating_cats_count;
static volatile int eating_mice_count;
static volatile char *Bowls;

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *globalCatMouseSem;
static struct cv **bowls_cv;
//static struct lock **bowls_lk;

static struct cv *globalCV;
static struct lock *globalLK;



/* 
 * The CatMouse simulation will call this function once before any cat or
 * mouse tries to each.
 *
 * You can use it to initialize synchronization and other variables.
 * 
 * parameters: the number of bowls
 */
void
catmouse_sync_init(int bowls)
{
  /* replace this default implementation with your own implementation of catmouse_sync_init */
        eating_cats_count = 0;
        eating_mice_count = 0;

        Bowls = kmalloc(bowls * sizeof(char));
        if (Bowls == NULL) {
                panic("catmouse_sync_init: not enough space for bowls\n");
        }

        bowls_cv = kmalloc(bowls * sizeof(struct cv *));
        if (bowls_cv == NULL) {
                panic("initialize_bowls: unable to allocate space for %d bowls\n", bowls);
        }

        for (int i = 0; i < bowls; ++i) {
                Bowls[i] = ' ';

                bowls_cv[i] = cv_create("bowlCV");
                if (bowls_cv[i] == NULL) {
                        panic("catmouse_sync_init: create bowl's cv fail");
                }
        }

        globalCV = cv_create("globalCV");
        if (globalCV == NULL) {
                panic("catmouse_sync_init: could not create globalCV");
        }

        globalLK = lock_create("globalLK");
        if (globalLK == NULL) {
                panic("catmouse_sync_init: could not create globalLK");
        }
  //(void)bowls; /* keep the compiler from complaining about unused parameters */
  /*globalCatMouseSem = sem_create("globalCatMouseSem",1);
  if (globalCatMouseSem == NULL) {
    panic("could not create global CatMouse synchronization semaphore");
  }*/
  return;
}

/* 
 * The CatMouse simulation will call this function once after all cat
 * and mouse simulations are finished.
 *
 * You can use it to clean up any synchronization and other variables.
 *
 * parameters: the number of bowls
 */
void
catmouse_sync_cleanup(int bowls)
{
  /* replace this default implementation with your own implementation of catmouse_sync_cleanup */
        for (int i = 0; i < bowls; ++i) {
                cv_destroy(bowls_cv[i]);
        }
        kfree(bowls_cv);
        kfree((void *)Bowls);
        cv_destroy(globalCV);
        lock_destroy(globalLK);
  //(void)bowls; /* keep the compiler from complaining about unused parameters */
  //KASSERT(globalCatMouseSem != NULL);
  //sem_destroy(globalCatMouseSem);
}


/*
 * The CatMouse simulation will call this function each time a cat wants
 * to eat, before it eats.
 * This function should cause the calling thread (a cat simulation thread)
 * to block until it is OK for a cat to eat at the specified bowl.
 *
 * parameter: the number of the bowl at which the cat is trying to eat
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
cat_before_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of cat_before_eating */
        lock_acquire(globalLK);
        while (true) {
                while (eating_mice_count != 0) {
                        cv_wait(globalCV, globalLK);
                }

                if (Bowls[bowl - 1] != ' ') {
                        cv_wait(bowls_cv[bowl - 1], globalLK);
                } else {
                        Bowls[bowl - 1] = 'x';
                        ++eating_cats_count;
                        lock_release(globalLK);
                        break;
                }
        }
  //(void)bowl;  /* keep the compiler from complaining about an unused parameter */
  //KASSERT(globalCatMouseSem != NULL);
  //P(globalCatMouseSem);
}

/*
 * The CatMouse simulation will call this function each time a cat finishes
 * eating.
 *
 * You can use this function to wake up other creatures that may have been
 * waiting to eat until this cat finished.
 *
 * parameter: the number of the bowl at which the cat is finishing eating.
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
cat_after_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of cat_after_eating */
        lock_acquire(globalLK);
        --eating_cats_count;
        Bowls[bowl - 1] = ' ';

        if (!eating_cats_count) {
                cv_broadcast(globalCV, globalLK);
                cv_broadcast(bowls_cv[bowl - 1], globalLK);
        } else {
                cv_signal(bowls_cv[bowl - 1], globalLK);
        }

        lock_release(globalLK);
  //(void)bowl;  /* keep the compiler from complaining about an unused parameter */
  //KASSERT(globalCatMouseSem != NULL);
  //V(globalCatMouseSem);
}

/*
 * The CatMouse simulation will call this function each time a mouse wants
 * to eat, before it eats.
 * This function should cause the calling thread (a mouse simulation thread)
 * to block until it is OK for a mouse to eat at the specified bowl.
 *
 * parameter: the number of the bowl at which the mouse is trying to eat
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
mouse_before_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of mouse_before_eating */
        lock_acquire(globalLK);
        while (true) {
                while (eating_cats_count != 0) {
                        cv_wait(globalCV, globalLK);
                }
                
                if (Bowls[bowl - 1] != ' ') {
                        cv_wait(bowls_cv[bowl - 1], globalLK);
                } else {
                        Bowls[bowl - 1] = 'x';
                        ++eating_mice_count;
                        lock_release(globalLK);
                        break;
                }
        }
  //(void)bowl;  /* keep the compiler from complaining about an unused parameter */
  //KASSERT(globalCatMouseSem != NULL);
  //P(globalCatMouseSem);
}

/*
 * The CatMouse simulation will call this function each time a mouse finishes
 * eating.
 *
 * You can use this function to wake up other creatures that may have been
 * waiting to eat until this mouse finished.
 *
 * parameter: the number of the bowl at which the mouse is finishing eating.
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
mouse_after_eating(unsigned int bowl) 
{
  /* replace this default implementation with your own implementation of mouse_after_eating */
        lock_acquire(globalLK);
        --eating_mice_count;
        Bowls[bowl - 1] = ' ';
        
        if (!eating_mice_count) {
                cv_broadcast(globalCV, globalLK);
                cv_broadcast(bowls_cv[bowl - 1], globalLK);
        } else {
                cv_signal(bowls_cv[bowl - 1], globalLK);
        }
        
        lock_release(globalLK);
  //(void)bowl;  /* keep the compiler from complaining about an unused parameter */
  //KASSERT(globalCatMouseSem != NULL);
  //V(globalCatMouseSem);
}
