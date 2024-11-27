#include <stdlib.h>
#include "array_tools.h"

// Use bubble sort to order array elements
int find_median(int arr[], int num_elem) {
    int swap_activated;  // If do not swap any elements, deed is done
    int temp;   // Used to swap 2 elements
    int stop_position;  // Stop position decreases each iteration

    // First iteration, stop comparing at end of array. Subsequently, the last element is correctly located
    for(stop_position=num_elem-1; stop_position>0; stop_position--) {
        swap_activated = 0;
        for(int i_swap=0; i_swap<stop_position; i_swap++) {
            //Compare elements at i and i+1. Swap if i is larger than i+1
            if (arr[i_swap] > arr[i_swap+1]) {
                temp = arr[i_swap];
                arr[i_swap] = arr[i_swap+1];
                arr[i_swap+1] = temp;
                swap_activated = 1;
            }
        }
        // No swap was found so must be done
        if (swap_activated == 0) {break;}
    }
    // Median is at middle of sorted array
    return arr[num_elem/2];
}

