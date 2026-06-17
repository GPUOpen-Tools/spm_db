# Streaming Performance Monitor Database (SPM DB)

## A shared component for handling collected SPM data.

The SPM DB provides structures for holding collected raw and derived SPM data,
a class to provide the ability to construct derived SPM data from raw data,
and a class to construct derived SPM data from a form similar to the one stored in the RDF and legacy RGP formats, intended for use in deserialization of those formats.



### The Raw SPM DB

The raw SPM DB contains raw counters organized by hardware block, then counter index, then block instance. They're accessible via a const reference to (and stored internally as) a map of maps of vectors, assuming that block instances will be dense in the form: Hardware block ID -> block-specific counter index -> block index -> counter.



Raw counters are stored as objects which provide access to their originating block, index, and instance, and their sample values. They also provide a method to query GPA for their name and description, given a GPA counter context valid for the capture hardware. Counter samples may be 16 or 32 bits.



Raw SPM DB objects are fairly simple but mutable: Counter samples may be added to the raw SPM DB by providing the data and metadata to an SPM DB instance. They may not be removed once added. Currently, when adding counters, we assume that for each block and counter index, counters will be added in order by their block instance.



### The Derived SPM DB

A derived DB is a more complex object. It is immutable once constructed, and construction is performed via builders (see below) to handle additional complexity.

The derived DB contains *derived counters* organized by their "canonical name". For counters defined by GPA, this is the name given by GPA. For custom counters, users should make them unique and consistent for each definition. The derived DB also contains a list of groups with names, descriptions and lists of pointers to counters, which remain valid for the life of the DB. These are implemented as pointers into the name->counter map.

A Derived counter contains the following information about a counter:

* A canonical name
  as described above
* A unit
  which applies to each sample value.
* A display name
  an arbitrary name for the counter which should be short but user-friendly.
  This is intended for use by tools as the primary name to display to a user in a UI.
* A description
  a longer description of the counter.
* A list of sample values
* A list of components
  Components are named links to other counters from the same DB.
  The name describes the relation of the component to the counter.
  The link is a pointer to a counter, valid for the life of the DB.



### Derived SPM builders

Derived counter DBs are constructed via builders and modifying them after construction is not supported.
Two types of builders are provided: one for constructing a derived DB given counter definitions and a raw DB,
and one for constructing a derived DB based on the SPM data stored by the RDF and RGP file formats.

The former takes a single Raw SPM DB and GPA counter context for the capturing hardware, and can be given any number of derived counter definitions.
The counter definitions may be either GPA counter definitions, which are defined by GPA names and computed by GPA, or custom counter definitions, which are defined by explicit formulas and a given canonical name.
Groups and components are defined using the canonical names of counters.

The latter takes counter samples and metadata, and a numeric index for each counter and uses those numeric indices for references to counters in group and component definitions. This mirrors the structure of serialized RGP or RDF chunk data, which also refer to counters by numeric index. These builders check the validity of each reference and do not include group members or counter components which refer to missing counters.

Currently not all counter name data is present in the serialized forms: They do not contain canonical GPA names for any counters, do not contain display names for counters used as components, and use the display name field for component counters to store their relation to their parent. A workaround for this is provided in the indexed builder. Ideally, we would provide a way to deserialize (and serialize) chunk data directly, but there are not currently any shared chunk definitions, and I would rather not use this library to define them.
The "additional utilities" contains a single method for computing derived SPM data given raw SPM data and a GPA context for the capture hardware. It currently uses a hardcoded set of counters and groups, but I would like to change this in the future, and also add the ability to compute custom counters.



#### A note on timing data

Timing data, including sample frequency and actual sample timestamps, is not part of either SPM DB; the DBs are responsible only for counter data. Timing data is simple and should be handled by the client, likely by whatever object owns the SPM DBs, as `RgpSpmDataProvider` in RGP.

