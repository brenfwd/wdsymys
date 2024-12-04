#include <stdlib.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct one_result {
	double time;
	const char *label;
 } one_result;

one_result *results = NULL;
int current_test = 0;
int allocated_results = 0;

void summarize(const char *name, int size, int iterations, int show_gmeans, int show_penalty ) {
	int i;
	double millions = ((double)(size) * iterations)/1000000.0;
	double total_absolute_times = 0.0;
	double gmean_ratio = 0.0;
	
	/* find longest label so we can adjust formatting
		12 = strlen("description")+1 */
	int longest_label_len = 12;
	for (i = 0; i < current_test; ++i) {
		int len = (int)strlen(results[i].label);
		if (len > longest_label_len)
			longest_label_len = len;
	}

	printf("\ntest %*s description   absolute   operations   ratio with\n", longest_label_len-12, " ");
	printf("number %*s time       per second   test0\n\n", longest_label_len, " ");

	for (i = 0; i < current_test; ++i)
		printf("%2i %*s\"%s\"  %5.2f sec   %5.2f M     %.2f\n",
				i,
				(int)(longest_label_len - strlen(results[i].label)),
				"",
				results[i].label,
				results[i].time,
				millions/results[i].time,
				results[i].time/results[0].time);

	// calculate total time
	for (i = 0; i < current_test; ++i) {
		total_absolute_times += results[i].time;
	}

	// report total time
	printf("\nTotal absolute time for %s: %.2f sec\n", name, total_absolute_times);

	if ( current_test > 1 && show_penalty ) {
	
		// calculate gmean of tests compared to baseline
		for (i = 1; i < current_test; ++i) {
			gmean_ratio += log(results[i].time/results[0].time);
		}
		
		// report gmean of tests as the penalty
		printf("\n%s Penalty: %.2f\n\n", name, exp(gmean_ratio/(current_test-1)));
	}

	// // reset the test counter so we can run more tests
	current_test = 0;
}

int main() {}