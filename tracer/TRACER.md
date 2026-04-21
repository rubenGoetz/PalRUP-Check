
# PalRUP Proof Tracer

This directory contains a reference implememtation of a PalRUP proof tracer. The tracer can be compiled into standalone libraries for both C and C++ and used under the same [license](../LICENSE) as PalRUP-Check itself.

## Build

To build the C/C++ library, set up PalRUP as described [here](../README.md). If only the tracer library is desired, it can be build with `make palrup_tracer`/`make palrup_tracer_hpp` for C/C++.

The library will be created in your local build directory and be named `libpalrup_tracer.a` and `libpalrup_tracer_hpp.a` for C and C++ respectively. 

## Usage

To use the PalRUP tracer library, integrate `palruo_tracer.h`/`palrup_tracer.hpp` into your C/C++ project and link the respective library as necessary.

## Case Study: CaDiCaL

In the following we will outline how to extend the popular SAT-solver [CaDiCaL](https://github.com/arminbiere/cadical) to log PalRUP proof fragments by creating a new subclass of `Tracer` as an adapter to this library. We will call this Adapter `class ExampleTracer : public Tracer`:
- extend ExampleTracer by an attribute `PalRUPTracer palrup_tracer`
- extend `ExampleTracer()` to call `palrup_tracer = PalRUPTracer()` and `~ExampleTracer()` to call `~PalRUPTracer()`
- if the ExampleTracer gets initialized before a formula is read, call `palrup_tracer_init_last_id()` after the formula is read to make sure no invalid IDs will be generated
- extend `add_derived_clause()` to call `c_tracer.log_clause_addition()`
- extend `delete_clause()` to call `c_tracer.log_clause_deletion()`
- if CaDiCaL is integrated into a clasue sharing solver as a solver backend extend the respective clause import logic to call `c_tracer.log_clause_deletion()`

When building CaDiCaL `libpalrup_tracer_hpp.a` will have to be linked. Additionaly, the contents of `libpalrup_tracer_hpp.a` will have to be added to `libcadical.a` in the build process.
