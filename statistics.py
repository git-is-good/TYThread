#!/usr/bin/env python

import sys

class Analyzer(object):
    def __init__(self, jump):
        self.jump = jump

    def analyze(self):
        currentJump = 0
        self.total_value = 0
        self.total_count = 0
        for line in sys.stdin:
            if currentJump == self.jump:
                # analyze
                self.total_value += float(line.split()[2])
                self.total_count += 1
                currentJump = 0
            else:
                currentJump += 1

    def __str__(self):
        return "Average: " + str(self.total_value / self.total_count) + ", from " + str(self.total_count) + " tests"

    __repr__ = __str__


def main():
    if len(sys.argv) > 1:
        ana = Analyzer(int(sys.argv[1]))
    else:
        ana = Analyzer(0)

    ana.analyze()
    print ana
        
if __name__ == "__main__":
    main()
