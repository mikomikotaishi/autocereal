# autocereal

This is a C++26 project which I'm currently using with a freshly-built gcc-16.
it's designed to give cereal the ability to serialize any class without
having to write load/save functions for them.

## What's here RIGHT NOW

Serializing structs with public members seems to work. Serializing shared pointers
of those structs seems to work. Needs more testing, but the concept appears to be
sound.

Serializing things with private members fails -- the private members are not
being retrieved by reflection. I suspect a compiler bug, will attempt to contact
the developer working on it.

Inheritance doesn't work -- this seems to be by design. I have some ideas on how
to fix this that I'll dive into. I'll update this readme when I have something.

# Limitations

Two main ones of note are due to not being able to hoist std::strings across the
consteval boundary. This code reads member information into arrays of characters.
So max identifier length and max class members are both 256 right now. This can
be changed in the autocereal.h file if you need more.

Additionally, (I think) I'm creating a copy of this array per data member during
saves. This would actually be a good place to use a per-class singleton, since
there will only ever be one class definition for a class.

# Using

Is easy within the limitations discussed. Just include the header file and
you should be able to serialize things without having to write load/save
methods for them. You may need additional cereal includes beyond the ones
I'm auto-including. Feel free to experiment with the header, I'll be putting
more learning projects out there as I write them.

# tldr

The general concept is sound. Needs more work, but I wanted to get this out
early. Reflection is rather awkward to use right now, but is also kinda... well...
sorcery.
