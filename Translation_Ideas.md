# Translation
## Approach
The following Ideas for Optimization + Translation come to mind:

1.) Using an intermediate representation (IR) for before the Protobuf messages. (start for that could be the plan_node translation)
We are translating:
- given Query JSON &rarr; IR &rarr; Protobuf
- our AST (nodes) &rarr; IR &rarr; Protobuf

2.) Using JSON directly (easier translation)
We are translating:
- Query JSON &rarr; Protobuf
- our AST &rarr; JSON Structure &rarr; Protobuf

3.) Method overloading
We are translating:
- Query JSON &rarr; Protobuf
- our AST &rarr; Protobuf

4.) Using the AST as the intermediate representation
- We only translate Query JSON &rarr; AST nodes

Version 1.) is a bit programming overhead but superior in many ways:

- if the structure changes we only need to change the IR
- we can include the materialize and position lists in the IR (they are not yet in the queries I think)
- there can also be done optimizations for the specialized steps on the IR (the second step of optimization)
- 2.) does generally not seem like a good idea since JSON structure is not directly like the AST and optimizations on a string are generally not ideal
- 3.) would be straight forward, but somewhere we need to add the specialized operations and the second optimization step
- 4.) Is almost like 1.) but we need to define where we add the special operations (which is again like 1.) with and IR)
- in the future other structures could be added
