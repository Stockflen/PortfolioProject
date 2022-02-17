#ifndef FUNCTIONS_H
#define FUNCTIONS_H

struct userCommand *createCommand(char *currLine);

void useBuiltin(struct userCommand *currCommand);

void otherCommands(struct userCommand *currCommand);

void processCommand(struct userCommand *currCommand);

char* checkExpansion(char *inputString);

void freeCommand(struct userCommand *currCommand);

void printCommand(struct userCommand* aCommand);

int getInput();

#endif