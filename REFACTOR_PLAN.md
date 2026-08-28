# Plan for refactoring PretextView -  Stage 1: Focus on `main.cpp`

To start, `PretextView.cpp` is difficult to maintain in its current state.
This file is in major need of refactoring and exhibits many issues
which make it extremely difficult to identify and fix any bugs.

I have created the `dev` branch as a base for refactoring, and will
create feature merge requests in the GitFlow strategy to this branch.

The work has been split up incrementally in the following stages, each
will become its own merge request, pending any fixes etc.

The changes to the implementation of individual functions will be
minimal, if at all. This stage is about code organisation.

## Merge request 1: Initial Sweep

The aim of this merge request is to make the subsequent changes easier
to implement. This will mostly be a cleanup of the 'low-hanging fruit'
to accommodate further changes.

1. Move includes to the top of the main file.

2. Remove commented-out blocks.

3. Translate Mandarin comments.

4. Extract constants to a header file, using `constexpr` rather than pre-proc macros.

5. Extract reusable utilities to an independent file.

6. Set up appropriate linking and building to a standard layout.

These changes should then set the stage for splitting the file
into modules.

Steps 1 and 2 will be combined into single merge requests to `dev`,
then the subsequent steps will be independent.

## Merge request 2: Move items into modules based on category

This merge request will modularise the system, and move the code out
of the main file into suitable areas based on their use cases.

1. Categorise remaining methods in the main file in terms of *UI*,
*Rendering*, *Data*, *Utilities*, and other categories that arise.

2. Produce modules for those methods and migrate them out of the
main file.

This will make the structure of the repository more maintainable.

## Merge request 3: Implement safe pointers

Using the application leads frequently to segmentation faults and
crashes, the cause is likely unsafe memory management.

The pattern `thread_pool* Thread_Pool` should be replaced with
smart pointers e.g. `std::unique_ptr`.

This pass will be identifying the pointers with unsafe memory
management and implementing clean initialisation (i.e. RAII). 

## Going forward

We will evaluate the impact on maintainability of the code
following the above changes, and the impact of the safe pointer
implementation on the segmentation issues before identifying
new directions.

The likely approach will be to generalise the approach to all
source files, and to implement more stringent OOP than is currently
part of the repository.
